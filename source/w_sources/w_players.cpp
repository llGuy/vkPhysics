#include <bits/stdint-intn.h>
#include <cstddef>
#include <math.h>
#include "common/event.hpp"
#include "common/player.hpp"
#include "world.hpp"
#include "net.hpp"
#include "ai.hpp"

#include "engine.hpp"
#include <common/log.hpp>
#include "renderer/input.hpp"
#include "renderer/renderer.hpp"
#include "w_internal.hpp"
#include <common/math.hpp>
#include <vulkan/vulkan_core.h>

static int32_t local_player;
static player_t *spectator;

int32_t w_local_player_index() {
    return local_player;
}

void w_clear_players_and_render_rsc() {
    // Clear rendering resources;
    clear_players();
}

void w_handle_input(
    game_input_t *game_input,
    float dt) {
    player_action_t actions;

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
            push_player_actions(spectator, &actions, 0);
        }
    }
    else {
        push_player_actions(spectator, &actions, 0);
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
    for (uint32_t i = 0; i < get_player_count(); ++i) {
        player_t *p = get_player(i);
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

    w_execute_player_actions(spectator, events);
}

void w_set_local_player(
    int32_t local_id) {
    local_player = local_id;
}

player_t *w_get_local_player() {
    if (local_player >= 0) {
        return get_player(local_player);
    }
    else {
        return NULL;
    }
}

player_t *w_get_spectator() {
    return spectator;
}

player_t *w_add_player_from_info(
    player_init_info_t *init_info) {
    player_t *p = add_player();
    fill_player_info(p, init_info);
        
    // If offline
    if (!init_info->client_name) {
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
        p->cached_player_actions = FL_MALLOC(player_action_t, PLAYER_MAX_ACTIONS_COUNT * 2);

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
        w_player_animated_instance_init(&p->render->animations);
    }

    // LOG_INFOV("Added player %i: %s\n", p->local_id, p->name);
    return p;
}

void w_reposition_spectator() {
    spectator->ws_position = w_get_startup_screen_data()->position;
    spectator->ws_view_direction = w_get_startup_screen_data()->view_direction;
    spectator->ws_up_vector = w_get_startup_screen_data()->up_vector;
}

void w_handle_spectator_mouse_movement() {
    spectator->ws_view_direction = w_update_spectator_view_direction(
        spectator->ws_view_direction);
}
