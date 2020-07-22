#include <bits/stdint-intn.h>
#include <cstddef>
#include <math.h>
#include "common/event.hpp"
#include "game/world.hpp"
#include "net.hpp"
#include "ai.hpp"

#include "engine.hpp"
#include <common/log.hpp>
#include "renderer/input.hpp"
#include "renderer/renderer.hpp"
#include "w_internal.hpp"
#include <common/math.hpp>
#include <vulkan/vulkan_core.h>

static players_t players;

stack_container_t<player_t *> &w_get_players() {
    return players.players;
}

int32_t w_local_player_index() {
    return players.local_player;
}

static vector3_t player_scale;

vector3_t w_get_player_scale() {
    return player_scale;
}

void w_player_world_init() {
    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        players.local_id_from_client_id[i] = -1;
    }

    players.local_player = -1;

    players.players.init(MAX_PLAYERS);
    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        players.players.data[i] = NULL;
    }

    players.spectator = FL_MALLOC(player_t, 1);
    memset(players.spectator, 0, sizeof(player_t));
    players.spectator->default_speed = 40.0f;
    players.spectator->ws_position = vector3_t(3.7f, -136.0f, -184.0f);
    players.spectator->ws_view_direction = vector3_t(0.063291, 0.438437, 0.896531);
    players.spectator->ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
    players.spectator->flags.interaction_mode = PIM_FLOATING;
    players.spectator->camera_fov.current = 60.0f;
    players.spectator->current_camera_up = vector3_t(0.0f, 1.0f, 0.0f);

    player_scale = vector3_t(0.5f);
}

void w_player_render_init(
    player_t *player) {
    player->render = FL_MALLOC(player_render_t, 1);
}

player_t *w_add_player() {
    uint32_t player_index = players.players.add();
    players.players[player_index] = FL_MALLOC(player_t, 1);
    player_t *p = players.players[player_index];
    memset(p, 0, sizeof(player_t));
    p->local_id = player_index;

    return p;
}

// TODO: May need to remove check in future, when it is sure that a player has a client id
void w_link_client_id_to_local_id(
    uint16_t client_id,
    uint32_t local_id ) {
    players.local_id_from_client_id[client_id] = local_id;
}

void w_handle_input(
    game_input_t *game_input,
    float dt) {
    player_actions_t actions;

    actions.bytes = 0;

    actions.accumulated_dt = 0.0f;

    actions.dt = dt;
    actions.dmouse_x = game_input->mouse_x - game_input->previous_mouse_x;
    actions.dmouse_y = game_input->mouse_y - game_input->previous_mouse_y;

    if (game_input->actions[GIAT_MOVE_FORWARD].state == BS_DOWN)
        actions.move_forward = 1;
    if (game_input->actions[GIAT_MOVE_LEFT].state == BS_DOWN)
        actions.move_left = 1;
    if (game_input->actions[GIAT_MOVE_BACK].state == BS_DOWN)
        actions.move_back = 1;
    if (game_input->actions[GIAT_MOVE_RIGHT].state == BS_DOWN)
        actions.move_right = 1;
    if (game_input->actions[GIAT_TRIGGER4].state == BS_DOWN)
        actions.jump = 1;
    if (game_input->actions[GIAT_TRIGGER6].state == BS_DOWN)
        actions.crouch = 1;
    if (game_input->actions[GIAT_TRIGGER1].state == BS_DOWN)
        actions.trigger_left = 1;
    if (game_input->actions[GIAT_TRIGGER2].state == BS_DOWN)
        actions.trigger_right = 1;
    if (game_input->actions[GIAT_TRIGGER5].instant == BS_DOWN)
        actions.switch_shapes = 1;
    if (game_input->actions[GIAT_TRIGGER7].instant == BS_DOWN)
        actions.flashlight = 1;

    actions.tick = get_current_tick();
    
    player_t *local_player = w_get_local_player();

    if (local_player) {
        if (local_player->flags.alive_state == PAS_ALIVE) {
            push_player_actions(local_player, &actions, 0);
        }
        else {
            push_player_actions(players.spectator, &actions, 0);
        }
    }
    else {
        push_player_actions(players.spectator, &actions, 0);
    }
}

static void s_interpolate_remote_player_snapshots(
    player_t *p) {
    // This adds a little delay
    // Makes sure that there is always snapshots to interpolate between
    if (p->remote_snapshots.head_tail_difference >= 3) {
        uint32_t previous_snapshot_index = p->remote_snapshots.tail;
        uint32_t next_snapshot_index = p->remote_snapshots.tail;

        if (++next_snapshot_index == p->remote_snapshots.buffer_size) {
            next_snapshot_index = 0;
        }

        p->elapsed += logic_delta_time();

        float progression = p->elapsed / server_snapshot_interval();
        
        // It is possible that progression went way past maximum (in case of extreme lag, so need to
        // take into account how many times over maximum time we went)
        if (progression >= 1.0f) {
            int32_t skip_count = (int32_t)(floor(progression));
            //progression = fmod(progression, 1.0f);
            progression -= (float)skip_count;
            p->elapsed -= server_snapshot_interval() * (float)skip_count;

            for (int32_t i = 0; i < skip_count; ++i) {
                p->remote_snapshots.get_next_item_tail();
            }

            previous_snapshot_index = p->remote_snapshots.tail;
            next_snapshot_index = p->remote_snapshots.tail;

            if (++next_snapshot_index == p->remote_snapshots.buffer_size) {
                next_snapshot_index = 0;
            }
        }

        player_snapshot_t *previous_snapshot = &p->remote_snapshots.buffer[previous_snapshot_index];
        player_snapshot_t *next_snapshot = &p->remote_snapshots.buffer[next_snapshot_index];

        p->ws_position = interpolate(previous_snapshot->ws_position, next_snapshot->ws_position, progression);
        p->ws_view_direction = interpolate(previous_snapshot->ws_view_direction, next_snapshot->ws_view_direction, progression);
        p->ws_up_vector = interpolate(previous_snapshot->ws_up_vector, next_snapshot->ws_up_vector, progression);

        p->flags.alive_state = previous_snapshot->alive_state;
    }
}

void w_tick_players(
    event_submissions_t *events) {
    // Handle networking stuff
    for (uint32_t i = 0; i < players.players.data_count; ++i) {
        player_t *p = players.players[i];
        if (p) {
            if (p->flags.alive_state == PAS_ALIVE) {
                // Will be 0 for remote players
                w_execute_player_actions(p, events);
            }

            if (p->flags.is_remote) {
                s_interpolate_remote_player_snapshots(p);
            }
        }
    }

    w_execute_player_actions(players.spectator, events);
}

void w_set_local_player(
    int32_t local_id) {
    players.local_player = local_id;
}


player_t *w_get_local_player() {
    if (players.local_player >= 0) {
        return players.players[players.local_player];
    }
    else {
        return NULL;
    }
}

player_t *w_get_spectator() {
    return players.spectator;
}

void w_destroy_player(
    uint32_t id) {
    player_t *p = players.players[id];
    if (p) {
        if (p->render) {
            FL_FREE(p->render);
        }

        players.players[id] = NULL;
        // TODO: Free const char *name?
        players.players.remove(id);

        FL_FREE(p);
    }
}

void w_clear_players() {
    players.local_player = -1;

    for (uint32_t i = 0; i < players.players.data_count; ++i) {
        w_destroy_player(i);
    }

    players.players.clear();

    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        players.local_id_from_client_id[i] = -1;
    }
}

player_t *w_add_player_from_info(
    player_init_info_t *init_info) {
    player_t *p = w_add_player();
        
    if (init_info->client_data) {
        p->name = init_info->client_data->name;
        p->client_id = init_info->client_data->client_id;
        // Now the network module can use w_get_player_from_client_id to get access to player directly
        w_link_client_id_to_local_id(p->client_id, p->local_id);
    }

    p->ws_position = init_info->ws_position;
    p->ws_view_direction = init_info->ws_view_direction;
    p->ws_up_vector = init_info->ws_up_vector;
    p->player_action_count = 0;
    p->default_speed = init_info->default_speed;
    p->next_random_spawn_position = init_info->next_random_spawn_position;
    p->ball_speed = 0.0f;
    p->ws_velocity = vector3_t(0.0f);
    p->frame_displacement = 0.0f;
    p->rotation_speed = 0.0f;
    p->rotation_angle = 0.0f;
    memset(p->player_actions, 0, sizeof(p->player_actions));

    p->accumulated_dt = 0.0f;

    p->flags.u32 = init_info->flags;

    // If offline
    if (!init_info->client_data) {
        p->flags.alive_state = PAS_ALIVE;
    }
    
    if (p->flags.is_local) {
        LOG_INFOV("%s is local\n", p->name);

        // If this is the local player (controlled by mouse and keyboard, need to cache all player actions to send to the server)
        // Only bind camera if player is alive
        if (p->flags.alive_state == PAS_ALIVE) {
            // This binds the camera
            w_set_local_player(p->local_id);
        }
        else {
            w_set_local_player(-1);
        }
        
        p->cached_player_action_count = 0;
        p->cached_player_actions = FL_MALLOC(player_actions_t, MAX_PLAYER_ACTIONS * 2);

        p->flags.is_remote = 0;
        p->flags.is_local = 1;
    }
    else {
        p->flags.is_remote = 1;
        p->flags.is_local = 0;

        // Initialise remote snapshots
        p->remote_snapshots.init();
        p->elapsed = 0.0f;
    }

    if (get_game_init_flags() | GIF_WINDOWED) {
        w_player_animated_instance_init(&p->animations);
    }

    // LOG_INFOV("Added player %i: %s\n", p->local_id, p->name);
    return p;
}

void w_begin_ai_training_players(
    ai_training_session_t type) {
    players.spectator->ws_position = vector3_t(0.0f, -4.0f, 0.0f);

    switch (type) {

    case ATS_WALKING: {
        player_init_info_t info = {};
        info.ws_position = vector3_t(0.0f);
        info.ws_view_direction = vector3_t(0.0f, 0.0f, -1.0f);
        info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
        info.default_speed = 15.0f;

        player_flags_t flags = {};
        flags.u32 = 0;
        flags.alive_state = PAS_ALIVE;
        flags.interaction_mode = PIM_STANDING;

        info.flags = flags.u32;

        player_t *p = w_add_player_from_info(
            &info);

        uint32_t ai_id = attach_ai(p->local_id, 0);
    } break;

    }
}

void w_reposition_spectator() {
    players.spectator->ws_position = w_get_startup_screen_data()->position;
    players.spectator->ws_view_direction = w_get_startup_screen_data()->view_direction;
    players.spectator->ws_up_vector = w_get_startup_screen_data()->up_vector;
}

void w_handle_spectator_mouse_movement() {
    players.spectator->ws_view_direction = w_update_spectator_view_direction(
        players.spectator->ws_view_direction);
}

player_t *get_player_from_client_id(
    uint16_t client_id) {
    int16_t id = players.local_id_from_client_id[client_id];
    if (id >= 0) {
        return players.players[id];
    }
    else {
        LOG_ERROR("Client ID not yet registered to local ID\n");
        return NULL;
    }
}

player_t *get_player_from_player_id(
    uint32_t player_id) {
    return players.players[player_id];
}

stack_container_t<player_t *> &DEBUG_get_players() {
    return players.players;
}

player_t *DEBUG_get_spectator() {
    return players.spectator;
}
