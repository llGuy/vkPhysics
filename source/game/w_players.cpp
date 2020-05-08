#include <math.h>
#include "net.hpp"
#include "engine.hpp"
#include <common/log.hpp>
#include "w_internal.hpp"
#include <common/math.hpp>
#include <glm/gtx/projection.hpp>

static vector3_t player_scale;

vector3_t w_get_player_scale() {
    return player_scale;
}

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
    world->spectator->flags.interaction_mode = PIM_FLOATING;
    world->spectator->camera_fov.current = 60.0f;
    world->spectator->current_camera_up = vector3_t(0.0f, 1.0f, 0.0f);
}

static mesh_t player_mesh;
static shader_t player_shader;

void w_players_data_init() {
    shader_binding_info_t mesh_info = {};
    load_mesh_internal(IM_SPHERE, &player_mesh, &mesh_info);

    // const char *shader_paths[] = {
    //     "shaders/SPV/untextured_mesh.vert.spv",
    //     "shaders/SPV/untextured_mesh.geom.spv",
    //     "shaders/SPV/untextured_mesh.frag.spv"
    // };

    // For now, just use smooth shaded sphere
    const char *shader_paths[] = {
        "shaders/SPV/mesh.vert.spv",
        "shaders/SPV/mesh.frag.spv"
    };

    player_shader = create_mesh_shader_color(
        &mesh_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | /*VK_SHADER_STAGE_GEOMETRY_BIT | */VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE);

    player_scale = vector3_t(0.5f);
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
        w_push_player_actions(local_player, &actions, 0);
    }
    else {
        w_push_player_actions(world->spectator, &actions, 0);
    }
}

#define TERRAFORMING_SPEED 500.0f

void w_push_player_actions(
    player_t *player,
    player_actions_t *action,
    bool override_adt) {
    if (player->player_action_count < MAX_PLAYER_ACTIONS) {
        if (override_adt) {
            player->player_actions[player->player_action_count++] = *action;
        }
        else {
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
    }
    else {
        // ...
        LOG_WARNING("Too many player actions\n");
    }
}

void push_player_actions(
    player_t *player,
    player_actions_t *actions,
    bool override_adt) {
    w_push_player_actions(
        player,
        actions,
        override_adt);
}

#define TERRAFORMING_RADIUS 3.0f

static void s_execute_player_triggers(
    player_t *player,
    player_actions_t *player_actions,
    world_t *world) {
    if (player_actions->trigger_left) {
#if NET_DEBUG
        LOG_INFOV("(Tick %llu) Terraforming (adt %f) p: %s d:%s\n", (unsigned long long)player_actions->tick, player_actions->accumulated_dt, glm::to_string(player->ws_position).c_str(), glm::to_string(player->ws_view_direction).c_str());
#endif

        w_terraform(
            TT_DESTROY,
            player->ws_position,
            player->ws_view_direction,
            10.0f,
            TERRAFORMING_RADIUS,
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
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt,
            world);
    }
}

static void s_execute_player_direction_change(
    player_t *player,
    player_actions_t *player_actions) {
    vector2_t delta = vector2_t(player_actions->dmouse_x, player_actions->dmouse_y);

    static constexpr float SENSITIVITY = 15.0f;

    vector3_t res = player->ws_view_direction;

    float x_angle = glm::radians(-delta.x) * SENSITIVITY * player_actions->dt;// *elapsed;
    float y_angle = glm::radians(-delta.y) * SENSITIVITY * player_actions->dt;// *elapsed;
                
    res = matrix3_t(glm::rotate(x_angle, player->ws_up_vector)) * res;
    vector3_t rotate_y = glm::cross(res, player->ws_up_vector);
    res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

    res = glm::normalize(res);
                
    player->ws_view_direction = res;
}

// For spectator
static void s_execute_player_floating_movement(
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

static void s_execute_player_movement(
    player_t *player,
    player_actions_t *actions) {
    static const float GRAVITY = 10.0f;
    
    movement_axes_t axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);

    vector3_t acceleration = vector3_t(0.0f);

    bool made_movement = 0;
    
    if (actions->move_forward) {
        acceleration += axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_left) {
        acceleration -= axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_back) {
        acceleration -= axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_right) {
        acceleration += axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->jump) {
        acceleration += axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->crouch) {
        acceleration -= axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }
        
    static const float ACCELERATION = 10.0f;
    
    //player->ws_velocity = axes.forward;
    
    if (made_movement && glm::dot(acceleration, acceleration) != 0.0f) {
        acceleration = glm::normalize(acceleration);
    }

    player->ws_velocity += acceleration * actions->dt * ACCELERATION;
    player->ws_velocity += -player->ws_up_vector * GRAVITY * actions->dt;

    if (player->flags.contact == PCS_ON_GROUND) {
        player->ws_velocity += -player->ws_velocity * actions->dt;
    }
    else {
    }

    terrain_collision_t collision = {};
    collision.ws_size = player_scale;
    collision.ws_position = player->ws_position;
    collision.ws_velocity = player->ws_velocity * actions->dt;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    vector3_t previous_position = player->ws_position;

    vector3_t ws_new_position = collide_and_slide(&collision) * player_scale;

    if (!collision.detected) {
        terrain_collision_t new_collision = {};
        new_collision.ws_size = player_scale;
        new_collision.ws_position = player->ws_position;
        new_collision.ws_velocity = player->ws_velocity * actions->dt;
        new_collision.es_position = new_collision.ws_position / new_collision.ws_size;
        new_collision.es_velocity = new_collision.ws_velocity / new_collision.ws_size;
        collide_and_slide(&new_collision);
    }

    player->ws_position = ws_new_position;
    //player->ws_velocity = (collision.es_velocity * player_scale) / actions->dt;

    if (collision.detected) {
        LOG_INFO("Collision detected\n");
        vector3_t normal = glm::normalize(collision.es_surface_normal * player_scale);

        if (player->flags.contact == PCS_IN_AIR) {
            if (glm::abs(glm::dot(player->ws_velocity, player->ws_velocity)) == 0.0f) {
                float previous_velocity_length = glm::length(player->ws_velocity);
                movement_axes_t new_axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);
                player->ws_velocity = glm::normalize(glm::proj(player->ws_velocity, new_axes.forward)) * previous_velocity_length * 0.2f;
            }
        }
        else {
            //player->ws_velocity = (player->ws_position - previous_position) / actions->dt;
        }

        player->next_camera_up = normal;
        player->ws_up_vector = normal;
        player->flags.contact = PCS_ON_GROUND;

        float vdotv = glm::dot(player->ws_velocity, player->ws_velocity);
        if (glm::abs(vdotv) == 0.0f) {
            player->ws_velocity = vector3_t(0.0f);
        }
        else {
            // Apply normal force
            player->ws_velocity += player->ws_up_vector * GRAVITY * actions->dt;
        }
    }
    else {
        LOG_INFO("No collision detected\n");
        player->flags.contact = PCS_IN_AIR;

        // LOG_INFO("In air\n");
    }
}

static void s_accelerate_meteorite_player(
    player_t *player,
    player_actions_t *actions) {
    // Need to set player's up vector depending on direction it is flying
    vector3_t right = glm::normalize(glm::cross(player->ws_view_direction, player->ws_up_vector));
    player->ws_up_vector = glm::normalize(glm::cross(right, player->ws_view_direction));

    player->next_camera_up = player->ws_up_vector;

    static const float METEORITE_ACCELERATION = 25.0f;
    static const float MAX_METEORITE_SPEED = 25.0f;

    // Apply air resistance
    player->ws_velocity -= (player->ws_velocity * 0.1f) * actions->dt;

    vector3_t final_velocity = player->ws_velocity * actions->dt;

    terrain_collision_t collision = {};
    collision.ws_size = player_scale;
    collision.ws_position = player->ws_position;
    collision.ws_velocity = final_velocity;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    player->ws_position = collide_and_slide(&collision) * player_scale;
    player->ws_velocity = (collision.es_velocity * player_scale) / actions->dt;

    if (collision.detected) {
        // Change interaction mode to ball
        player->flags.interaction_mode = PIM_BALL;
        player->camera_distance.set(1, 10.0f, player->camera_distance.current);
        player->camera_fov.set(1, 60.0f, player->camera_fov.current);

        vector3_t normal = glm::normalize(collision.es_surface_normal * player_scale);
        
        player->next_camera_up = normal;
        player->ws_up_vector = normal;

        player->flags.contact = PCS_ON_GROUND;
    }
    else {
        player->flags.contact = PCS_IN_AIR;

        if (glm::dot(player->ws_velocity, player->ws_velocity) < MAX_METEORITE_SPEED * MAX_METEORITE_SPEED) {
            //player->meteorite_speed += METEORITE_ACCELERATION * actions->dt;
            player->ws_velocity += player->ws_view_direction * METEORITE_ACCELERATION * actions->dt;
        }
        else {
            player->ws_velocity = glm::normalize(player->ws_velocity) * MAX_METEORITE_SPEED;
        }
    }
}

static void s_execute_player_actions(
    player_t *player,
    world_t *world) {

    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        player_actions_t *actions = &player->player_actions[i];

        switch (player->flags.interaction_mode) {
            
        case PIM_METEORITE: {
            s_execute_player_direction_change(player, actions);
            s_accelerate_meteorite_player(player, actions);
        } break;


            // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
        case PIM_STANDING: {
            s_execute_player_direction_change(player, actions);
            s_execute_player_movement(player, actions);
            s_execute_player_triggers(player, actions, world);
        } break;

        case PIM_BALL: {
            s_execute_player_direction_change(player, actions);
            s_execute_player_movement(player, actions);
            s_execute_player_triggers(player, actions, world);
        } break;

        case PIM_FLOATING: {
            s_execute_player_direction_change(player, actions);
            s_execute_player_floating_movement(player, actions);
            s_execute_player_triggers(player, actions, world);
        } break;

        default: {
        } break;

        }

        // If this is local player, need to cache these commands to later send to server
        if ((int32_t)player->local_id == world->local_player && connected_to_server()) {
            player->camera_distance.animate(actions->dt);
            player->camera_fov.animate(actions->dt);

            vector3_t up_diff = player->next_camera_up - player->current_camera_up;
            if (glm::dot(up_diff, up_diff) > 0.00001f) {
                player->current_camera_up = glm::normalize(player->current_camera_up + up_diff * actions->dt * 2.0f);
            }

            if (connected_to_server()) {
                if (player->cached_player_action_count < MAX_PLAYER_ACTIONS * 2) {
                    player->cached_player_actions[player->cached_player_action_count++] = *actions;
                }
                else {
                    LOG_WARNING("Too many cached player actions\n");
                }
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
    world_t *world) {
    // Handle networking stuff
    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        player_t *p = world->players[i];
        if (p) {
            // Will be 0 for remote players
            s_execute_player_actions(p, world);

            if (p->flags.is_remote) {
                s_interpolate_remote_player_snapshots(p);
            }
        }
    }

    s_execute_player_actions(world->spectator, world);
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
            if (p->flags.alive_state == PAS_ALIVE) {
                if (!p->render) {
                    w_player_render_init(p);
                }

                p->render->render_data.model = glm::translate(p->ws_position) * glm::scale(player_scale);
                p->render->render_data.color = vector4_t(1.0f);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)world->local_player) {
                    if (p->flags.camera_type == CT_THIRD_PERSON) {
                        submit_mesh(
                            render_command_buffer,
                            &player_mesh,
                            &player_shader,
                            &p->render->render_data);
                    }
                }
                else {
                    submit_mesh(
                        render_command_buffer,
                        &player_mesh,
                        &player_shader,
                        &p->render->render_data);
                }
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
