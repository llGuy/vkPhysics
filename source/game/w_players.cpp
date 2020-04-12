#include <math.h>
#include "net.hpp"
#include "engine.hpp"
#include <common/log.hpp>
#include "w_internal.hpp"
#include <common/math.hpp>

void w_player_world_init(
    world_t *world) {
    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        world->local_id_from_client_id[i] = -1;
    }

    world->local_player = -1;

    world->players.init(MAX_PLAYERS);
    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        world->players.data[i] = NULL;
    }

    world->spectator = FL_MALLOC(player_t, 1);
    memset(world->spectator, 0, sizeof(player_t));
    world->spectator->default_speed = 20.0f;
    world->spectator->ws_position = vector3_t(-50.0f, 0.0f, -50.0f);
    world->spectator->ws_view_direction = vector3_t(1.0f, 0.0f, 0.0f);
    world->spectator->ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
}

static mesh_t player_mesh;
static shader_t player_shader;

void w_players_data_init() {
    shader_binding_info_t mesh_info = {};
    load_mesh_internal(IM_CUBE, &player_mesh, &mesh_info);

    const char *shader_paths[] = {
        "shaders/SPV/untextured_mesh.vert.spv",
        "shaders/SPV/untextured_mesh.geom.spv",
        "shaders/SPV/untextured_mesh.frag.spv"
    };

    player_shader = create_mesh_shader_color(
        &mesh_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE);
}

void w_player_render_init(
    player_t *player) {
    player->render = FL_MALLOC(player_render_t, 1);
}

player_t *w_add_player(
    world_t *world) {
    uint32_t player_index = world->players.add();
    world->players[player_index] = FL_MALLOC(player_t, 1);
    player_t *p = world->players[player_index];
    memset(p, 0, sizeof(player_t));
    p->local_id = player_index;

    return p;
}

// TODO: May need to remove check in future, when it is sure that a player has a client id
player_t *w_get_player_from_client_id(
    uint16_t client_id,
    world_t *world) {
    int16_t id = world->local_id_from_client_id[client_id];
    if (id >= 0) {
        return world->players[id];
    }
    else {
        LOG_ERROR("Client ID not yet registered to local ID\n");
        return NULL;
    }
}

void w_link_client_id_to_local_id(
    uint16_t client_id,
    uint32_t local_id,
    world_t *world) {
    world->local_id_from_client_id[client_id] = local_id;
}

void w_handle_input(
    game_input_t *game_input,
    float dt,
    world_t *world) {
    player_actions_t actions;

    actions.bytes = 0;

    actions.accumulated_dt = 0.0f;

    actions.dt = dt;
    actions.dmouse_x = game_input->mouse_x - game_input->previous_mouse_x;
    actions.dmouse_y = game_input->mouse_y - game_input->previous_mouse_y;

    if (game_input->actions[GIAT_MOVE_FORWARD].state == BS_DOWN) {
        actions.move_forward = 1;
    }
    
    if (game_input->actions[GIAT_MOVE_LEFT].state == BS_DOWN) {
        actions.move_left = 1;
    }
    
    if (game_input->actions[GIAT_MOVE_BACK].state == BS_DOWN) {
        actions.move_back = 1;
    }
    
    if (game_input->actions[GIAT_MOVE_RIGHT].state == BS_DOWN) {
        actions.move_right = 1;
    }
    
    if (game_input->actions[GIAT_TRIGGER4].state == BS_DOWN) { // Space
        actions.jump = 1;
    }
    
    if (game_input->actions[GIAT_TRIGGER6].state == BS_DOWN) { // Left shift
        actions.crouch = 1;
    }

    if (game_input->actions[GIAT_TRIGGER1].state == BS_DOWN) {
        actions.trigger_left = 1;
    }
        
    if (game_input->actions[GIAT_TRIGGER2].state == BS_DOWN) {
        actions.trigger_right = 1;
    }

    actions.tick = get_current_tick();
    
    player_t *local_player = w_get_local_player(world);
    if (local_player) {
        w_push_player_actions(local_player, &actions);
    }
    else {
        w_push_player_actions(world->spectator, &actions);
    }
}

#define TERRAFORMING_SPEED 400.0f

void w_push_player_actions(
    player_t *player,
    player_actions_t *action) {
    if (player->player_action_count < MAX_PLAYER_ACTIONS) {
        player->accumulated_dt += action->dt;

        float max_change = 1.0f * player->accumulated_dt * TERRAFORMING_SPEED;
        int32_t max_change_i = (int32_t)max_change;

        if (max_change_i < 2) {
            action->accumulated_dt = 0.0f;
        }
        else {
            action->accumulated_dt = player->accumulated_dt;
            player->accumulated_dt = 0.0f;
        }

        player->player_actions[player->player_action_count++] = *action;
    }
    else {
        // ...
        LOG_WARNING("Too many player actions\n");
    }
}

void push_player_actions(
    player_t *player,
    player_actions_t *actions) {
    w_push_player_actions(
        player,
        actions);
}

static void s_execute_player_triggers(
    player_t *player,
    player_actions_t *player_actions,
    world_t *world) {
    if (player_actions->trigger_left) {
        w_terraform(
            TT_DESTROY,
            player->ws_position,
            player->ws_view_direction,
            10.0f,
            3.0f,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt,
            world);
    }
    if (player_actions->trigger_right) {
        w_terraform(
            TT_BUILD,
            player->ws_position,
            player->ws_view_direction,
            10.0f,
            3.0f,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt,
            world);
    }
}

static void s_execute_player_direction_change(
    player_t *player,
    player_actions_t *player_actions) {
    vector2_t delta = vector2_t(player_actions->dmouse_x, player_actions->dmouse_y);

    static constexpr float SENSITIVITY = 30.0f;
    
    vector3_t res = player->ws_view_direction;
	    
    float x_angle = glm::radians(-delta.x) * SENSITIVITY * player_actions->dt;// *elapsed;
    float y_angle = glm::radians(-delta.y) * SENSITIVITY * player_actions->dt;// *elapsed;
                
    res = matrix3_t(glm::rotate(x_angle, vector3_t(0.0f, 1.0f, 0.0f))) * res;
    vector3_t rotate_y = glm::cross(res, vector3_t(0.0f, 1.0f, 0.0f));
    res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

    res = glm::normalize(res);
                
    player->ws_view_direction = res;
}

static void s_execute_player_movement(
    player_t *player,
    player_actions_t *actions) {
    vector3_t right = glm::normalize(glm::cross(player->ws_view_direction, player->ws_up_vector));
    vector3_t forward = glm::normalize(glm::cross(player->ws_up_vector, right));
    
    if (actions->move_forward) {
        player->ws_position += forward * actions->dt * player->default_speed;
    }
    if (actions->move_left) {
        player->ws_position -= right * actions->dt * player->default_speed;
    }
    if (actions->move_back) {
        player->ws_position -= forward * actions->dt * player->default_speed;
    }
    if (actions->move_right) {
        player->ws_position += right * actions->dt * player->default_speed;
    }
    if (actions->jump) {
        player->ws_position += player->ws_up_vector * actions->dt * player->default_speed;
    }
    if (actions->crouch) {
        player->ws_position -= player->ws_up_vector * actions->dt * player->default_speed;
    }
}

static void w_execute_player_actions(
    player_t *player,
    world_t *world) {
    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        player_actions_t *actions = &player->player_actions[i];
        s_execute_player_direction_change(player, actions);
        s_execute_player_movement(player, actions);
        s_execute_player_triggers(player, actions, world);

        // If this is local player, need to cache these commands to later send to server
        if ((int32_t)player->local_id == world->local_player && connected_to_server()) {
            if (player->cached_player_action_count < MAX_PLAYER_ACTIONS * 2) {
                player->cached_player_actions[player->cached_player_action_count++] = *actions;
            }
            else {
                LOG_WARNING("Too many cached player actions\n");
            }
        }
    }

    player->player_action_count = 0;
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
                p->remote_snapshots.get_next_item();
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
    }
}

void w_tick_players(
    world_t *world) {
    // Handle networking stuff
    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        player_t *p = world->players[i];
        if (p) {
            // Will be 0 for remote players
            w_execute_player_actions(p, world);

            if (p->is_remote) {
                s_interpolate_remote_player_snapshots(p);
            }
        }
    }

    w_execute_player_actions(world->spectator, world);
}

void w_set_local_player(
    int32_t local_id,
    world_t *world) {
    world->local_player = local_id;
}

// When skeletal animation is implemented, this function will do stuff like handle that
void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    struct world_t *world) {
    (void)transfer_command_buffer;
    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        player_t *p = world->players[i];
        if (p) {
            if (!p->render) {
                w_player_render_init(p);
            }

            p->render->render_data.model = glm::translate(p->ws_position);
            p->render->render_data.color = vector4_t(1.0f);
            p->render->render_data.pbr_info.x = 0.1f;
            p->render->render_data.pbr_info.y = 0.1f;

            if ((int32_t)i != (int32_t)world->local_player) {
                // Handle difference between rendering animations (and first / third person)
                submit_mesh(
                    render_command_buffer,
                    &player_mesh,
                    &player_shader,
                    &p->render->render_data);
            }
        }
    }
}

player_t *w_get_local_player(
    world_t *world) {
    if (world->local_player >= 0) {
        return world->players[world->local_player];
    }
    else {
        return NULL;
    }
}

player_t *w_get_spectator(
    world_t *world) {
    return world->spectator;
}

void w_destroy_player(
    uint32_t id,
    world_t *world) {
    player_t *p = world->players[id];
    if (p) {
        if (p->render) {
            FL_FREE(p->render);
        }

        world->players[id] = NULL;
        // TODO: Free const char *name?
        world->players.remove(id);

        FL_FREE(p);
    }
}

void w_clear_players(
    world_t *world) {
    world->local_player = -1;

    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        w_destroy_player(i, world);
    }

    world->players.clear();

    for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
        world->local_id_from_client_id[i] = -1;
    }
}
