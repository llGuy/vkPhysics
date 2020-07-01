#include <cstddef>
#include <math.h>
#include "common/event.hpp"
#include "game/world.hpp"
#include "net.hpp"
#include "ai.hpp"

#include "engine.hpp"
#include <common/log.hpp>
#include "renderer/renderer.hpp"
#include "w_internal.hpp"
#include <common/math.hpp>
#include <glm/gtx/projection.hpp>

static vector3_t player_scale;

vector3_t w_get_player_scale() {
    return player_scale;
}

// DEBUGGING
static animated_instance_t test_instance0;

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
    world->spectator->ws_position = vector3_t(3.7f, -136.0f, -184.0f);
    world->spectator->ws_view_direction = vector3_t(0.063291, 0.438437, 0.896531);
    world->spectator->ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
    world->spectator->flags.interaction_mode = PIM_FLOATING;
    world->spectator->camera_fov.current = 60.0f;
    world->spectator->current_camera_up = vector3_t(0.0f, 1.0f, 0.0f);
}

static mesh_t player_mesh;
static skeleton_t player_skeleton;
static animation_cycles_t player_cycles;
static shader_t player_shader;
static shader_t player_shadow_shader;

static mesh_t player_ball_mesh;
static shader_t player_ball_shader;
static shader_t player_ball_shadow_shader;

void w_player_animation_init(
    player_t *player) {
    animated_instance_init(&player->animations, &player_skeleton, &player_cycles);
}

void w_players_data_init() {
    shader_binding_info_t mesh_info = {};
    load_mesh_external(&player_mesh, &mesh_info, "assets/models/player.mesh");
    load_skeleton(&player_skeleton, "assets/models/player.skeleton");
    load_animation_cycles(&player_cycles, "assets/models/player.animations.link", "assets/models/player.animations");

    const char *shader_paths[] = {
        "shaders/SPV/skeletal.vert.spv",
        "shaders/SPV/skeletal.geom.spv",
        "shaders/SPV/skeletal.frag.spv"
    };

    player_shader = create_mesh_shader_color(
        &mesh_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE, 
        MT_ANIMATED);

    const char *shadow_shader_paths[] = {
        "shaders/SPV/skeletal_shadow.vert.spv",
        "shaders/SPV/shadow.frag.spv"
    };

    player_shadow_shader = create_mesh_shader_shadow(
        &mesh_info,
        shadow_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        MT_ANIMATED);

    const char *static_shader_paths[] = {
        "shaders/SPV/mesh.vert.spv",
        "shaders/SPV/mesh.frag.spv"
    };

    shader_binding_info_t ball_mesh_info = {};
    load_mesh_internal(IM_SPHERE, &player_ball_mesh, &ball_mesh_info);
    player_ball_shader = create_mesh_shader_color(
        &ball_mesh_info,
        static_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_NONE,
        MT_STATIC);

    const char *static_shadow_shader_path[] = {
        "shaders/SPV/mesh_shadow.vert.spv",
        "shaders/SPV/shadow.frag.spv"
    };

    player_ball_shadow_shader = create_mesh_shader_shadow(
        &ball_mesh_info,
        static_shadow_shader_path,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        MT_STATIC);

    player_scale = vector3_t(0.5f);

    animated_instance_init(&test_instance0, &player_skeleton, &player_cycles);
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

    if (game_input->actions[GIAT_TRIGGER5].instant == BS_DOWN) {
        actions.switch_shapes = 1;
    }
    
    if (game_input->actions[GIAT_TRIGGER7].instant == BS_DOWN) {
        actions.flashlight = 1;
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

#define TERRAFORMING_SPEED 200.0f

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
    player->terraform_package = w_cast_terrain_ray(
        player->ws_position,
        player->ws_view_direction,
        10.0f,
        world);

    if (player_actions->trigger_left) {
#if NET_DEBUG
        LOG_INFOV("(Tick %llu) Terraforming (adt %f) p: %s d:%s\n",
                  (unsigned long long)player_actions->tick,
                  player_actions->accumulated_dt,
                  glm::to_string(player->ws_position).c_str(),
                  glm::to_string(player->ws_view_direction).c_str());
#endif

        w_terraform(
            TT_DESTROY,
            player->terraform_package,
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt,
            world);
    }
    if (player_actions->trigger_right) {
        w_terraform(
            TT_BUILD,
            player->terraform_package,
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt,
            world);
    }

    if (player_actions->flashlight) {
        player->flags.flashing_light ^= 1;
    }
}

static void s_execute_player_direction_change(
    player_t *player,
    player_actions_t *player_actions) {
    vector2_t delta = vector2_t(player_actions->dmouse_x, player_actions->dmouse_y);

    static constexpr float SENSITIVITY = 15.0f;

    vector3_t res = player->ws_view_direction;

    float x_angle = glm::radians(-delta.x) * SENSITIVITY * player_actions->dt;
    float y_angle = glm::radians(-delta.y) * SENSITIVITY * player_actions->dt;
                
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

enum movement_resolution_flags_t {
    MRF_ADOPT_GRAVITY = 1 << 0,
    MRF_GRAVITY_CHECK_INCLINATION = 1 << 1,
    MRF_ABRUPT_STOP = 1 << 2,
    MRF_CAP_SPEED = 1 << 3,
};

struct force_values_t {
    // This will be very high for standing mode
    float friction;
    // This will be very high for standing mode too (maybe?)
    float movement_acceleration;
    float gravity;
    float maximum_walking_speed;
};

enum movement_change_type_t {
    MCT_STOP, MCT_TOO_FAST, MCT_MADE_MOVEMENT
};

static bool s_create_acceleration_vector(
    player_t *player,
    player_actions_t *actions,
    movement_resolution_flags_t flags,
    force_values_t *force_values,
    vector3_t *acceleration) {
    movement_axes_t axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);

    bool made_movement = 0;
    vector3_t final_acceleration = vector3_t(0.0f);
    
    if (actions->move_forward) {
        final_acceleration += axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_left) {
        final_acceleration -= axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_back) {
        final_acceleration -= axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_right) {
        final_acceleration += axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->jump) {
        final_acceleration += axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->crouch) {
        final_acceleration -= axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }

    *acceleration = final_acceleration;

    return made_movement;
}

static void s_apply_forces(
    player_t *player,
    player_actions_t *actions,
    force_values_t *force_values,
    movement_resolution_flags_t flags,
    const vector3_t &movement_acceleration) {
    player->ws_velocity += movement_acceleration * actions->dt * force_values->movement_acceleration;

    // Do some check between standing and ball mode
    if (flags & MRF_GRAVITY_CHECK_INCLINATION) {
        if (glm::dot(player->ws_up_vector, player->ws_surface_normal) < 0.7f || player->flags.contact == PCS_IN_AIR) {
            player->ws_velocity += -player->ws_up_vector * force_values->gravity * actions->dt;
        }
        else {
        }
    }
    else {
        player->ws_velocity += -player->ws_up_vector * force_values->gravity * actions->dt;
    }

    if (player->flags.contact == PCS_ON_GROUND) {
        player->ws_velocity += -player->ws_velocity * actions->dt * force_values->friction;
    }
    else {
    }
}

static terrain_collision_t s_resolve_player_movement(
    player_t *player,
    player_actions_t *actions,
    force_values_t *force_values,
    int32_t flags) {
    vector3_t acceleration = vector3_t(0.0f);
    bool made_movement = s_create_acceleration_vector(player, actions, (movement_resolution_flags_t)flags, force_values, &acceleration);
        
    if (made_movement && glm::dot(acceleration, acceleration) != 0.0f) {
        acceleration = glm::normalize(acceleration);
        player->flags.moving = 1;
    }
    else {
        player->flags.moving = 0;
    }
    if (!made_movement && player->flags.contact == PCS_ON_GROUND && flags & MRF_ABRUPT_STOP) {
        //player->ws_velocity = vector3_t(0.0f);
    }

    s_apply_forces(player, actions, force_values, (movement_resolution_flags_t)flags, acceleration);

    terrain_collision_t collision = {};
    collision.ws_size = player_scale;
    collision.ws_position = player->ws_position;
    collision.ws_velocity = player->ws_velocity * actions->dt;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    vector3_t ws_new_position = w_collide_and_slide(&collision) * player_scale;

    player->ws_position = ws_new_position;
    //player->ws_velocity = (collision.es_velocity * player_scale) / actions->dt;

    if (collision.detected) {
        //LOG_INFO("Collision detected\n");
        vector3_t normal = glm::normalize(collision.es_surface_normal * player_scale);
        player->ws_surface_normal = normal;

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


        if (flags & MRF_ADOPT_GRAVITY) {
            player->ws_up_vector = normal;
            player->next_camera_up = normal;
        }

        float vdotv = glm::dot(player->ws_velocity, player->ws_velocity);
        if (glm::abs(vdotv) == 0.0f) {
            player->ws_velocity = vector3_t(0.0f);
        }
        else {
            // Apply normal force
            player->ws_velocity += player->ws_up_vector * force_values->gravity * actions->dt;
        }
        player->flags.contact = PCS_ON_GROUND;
    }
    else {
        //LOG_INFO("No collision detected\n");
        player->flags.contact = PCS_IN_AIR;

        // LOG_INFO("In air\n");
    }
}

static void s_execute_standing_player_movement(
    player_t *player,
    player_actions_t *actions) {
    if (player->flags.contact == PCS_ON_GROUND) {
        if (actions->move_forward) {
            player->animated_state = PAS_RUNNING;
        }
        else {
            player->animated_state = PAS_IDLE;
        }
    }

    force_values_t forces = {};
    forces.friction = 9.0f;
    forces.movement_acceleration = 8.0f;
    forces.gravity = 10.0f;
    forces.maximum_walking_speed = player->default_speed * 0.5f;

    s_resolve_player_movement(
        player,
        actions,
        &forces,
        MRF_GRAVITY_CHECK_INCLINATION | MRF_ABRUPT_STOP | MRF_CAP_SPEED);
}

static void s_execute_ball_player_movement(player_t *player, player_actions_t *actions) {
    force_values_t forces = {};
    forces.friction = 1.0f;
    forces.movement_acceleration = 10.0f;
    forces.gravity = 10.0f;
    forces.maximum_walking_speed = player->default_speed;
    s_resolve_player_movement(player, actions, &forces, MRF_ADOPT_GRAVITY | MRF_CAP_SPEED);
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

    player->ws_position = w_collide_and_slide(&collision) * player_scale;
    player->ws_velocity = (collision.es_velocity * player_scale) / actions->dt;

    if (collision.detected) {
        // Change interaction mode to ball
        player->flags.interaction_mode = PIM_BALL;
        player->camera_distance.set(1, 10.0f, player->camera_distance.current, 1.0f);
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

static void s_check_player_dead(
    player_actions_t *actions,
    world_t *world,
    player_t *player,
    event_submissions_t *events) {
    // Start checking for death, if the velocity vector
    // is pretty aligned with the up vector (but down) of the player
    // because if it is, that means that the velocity vector won't be able to change too much.
    // (gravity has already pulled the player out of any sort of movement range)
    if (player->flags.contact == PCS_IN_AIR) {
        vector3_t normalized_velocity = glm::normalize(player->ws_velocity);

        float vdu = glm::dot(normalized_velocity, glm::normalize(-player->ws_up_vector));

        if (vdu > 0.9f) {
            // Do ray cast to check if there are chunks underneath the player
            uint32_t ray_step_count = 10;
            vector3_t current_ray_position = player->ws_position;
            current_ray_position += 16.0f * normalized_velocity;
            for (uint32_t i = 0; i < ray_step_count; ++i) {
                ivector3_t chunk_coord = w_convert_voxel_to_chunk(w_convert_world_to_voxel(current_ray_position));

                chunk_t *c = w_access_chunk(chunk_coord, world);
                if (c) {
                    // Might not die, reset timer
                    player->death_checker = 0.0f;

                    break;
                }

                current_ray_position += 16.0f * normalized_velocity;
            }

            player->death_checker += actions->dt;

            if (player->death_checker > 5.0f) {
                // The player is dead
                LOG_INFO("Player has just died\n");

                player->flags.alive_state = PAS_DEAD;

                submit_event(ET_LAUNCH_GAME_MENU_SCREEN, NULL, events);
                submit_event(ET_BEGIN_RENDERING_SERVER_WORLD, NULL, events);
            }
        }
    }
    else {
        player->death_checker = 0.0f;
    }
}

static void s_execute_player_actions(
    player_t *player,
    world_t *world,
    event_submissions_t *events) {
    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        player_actions_t *actions = &player->player_actions[i];

        if (actions->switch_shapes) {
            if (player->flags.interaction_mode == PIM_STANDING) {
                player->flags.interaction_mode = PIM_BALL;
                //player->camera_distance.set(1, 10.0f, player->camera_distance.current, 7.0f);
            }
            else if (player->flags.interaction_mode == PIM_BALL) {
                player->flags.interaction_mode = PIM_STANDING;
                //player->camera_distance.set(1, 0.0f, player->camera_distance.current, 7.0f);
            }
        }

        switch (player->flags.interaction_mode) {
            
        case PIM_METEORITE: {
            s_execute_player_direction_change(player, actions);
            s_accelerate_meteorite_player(player, actions);
        } break;

            // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
        case PIM_STANDING: {
            s_execute_player_triggers(player, actions, world);
            s_execute_player_direction_change(player, actions);
            s_execute_standing_player_movement(player, actions);

            s_check_player_dead(actions, world, player, events);
        } break;

        case PIM_BALL: {
            s_execute_player_direction_change(player, actions);
            s_execute_ball_player_movement(player, actions);

            s_check_player_dead(actions, world, player, events);
        } break;

        case PIM_FLOATING: {
            s_execute_player_triggers(player, actions, world);
            s_execute_player_direction_change(player, actions);
            s_execute_player_floating_movement(player, actions);
        } break;

        default: {
        } break;

        }

        // If this is local player, need to cache these commands to later send to server
        if ((int32_t)player->local_id == world->local_player && connected_to_server()) {
            player->camera_distance.animate(actions->dt);
            player->camera_fov.animate(actions->dt);

            world->local_current_terraform_package = player->terraform_package;
            if (player->flags.interaction_mode != PIM_STANDING) {
                world->local_current_terraform_package.ray_hit_terrain = 0;
            }

            vector3_t up_diff = player->next_camera_up - player->current_camera_up;
            if (glm::dot(up_diff, up_diff) > 0.00001f) {
                player->current_camera_up = glm::normalize(player->current_camera_up + up_diff * actions->dt * 1.5f);
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

        if (player->flags.alive_state == PAS_DEAD) {
            break;
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
    world_t *world,
    event_submissions_t *events) {
    // Handle networking stuff
    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        player_t *p = world->players[i];
        if (p) {
            if (p->flags.alive_state == PAS_ALIVE) {
                // Will be 0 for remote players
                s_execute_player_actions(p, world, events);
            }

            if (p->flags.is_remote) {
                s_interpolate_remote_player_snapshots(p);
            }
        }
    }

    s_execute_player_actions(world->spectator, world, events);
}

void w_set_local_player(
    int32_t local_id,
    world_t *world) {
    world->local_player = local_id;
}

static void s_render_person(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    player_t *p) {
    if (p->animations.next_bound_cycle != p->animated_state) {
        switch_to_cycle(
            &p->animations,
            p->animated_state,
            0);
    }

    interpolate_joints(&p->animations, logic_delta_time());
    sync_gpu_with_animated_transforms(&p->animations, transfer_command_buffer);

    // This has to be a bit different
    movement_axes_t axes = compute_movement_axes(p->ws_view_direction, p->ws_up_vector);
    matrix3_t normal_rotation_matrix3 = (matrix3_t(glm::normalize(axes.right), glm::normalize(axes.up), glm::normalize(-axes.forward)));
    matrix4_t normal_rotation_matrix = matrix4_t(normal_rotation_matrix3);
    normal_rotation_matrix[3][3] = 1.0f;

    vector3_t view_dir = glm::normalize(p->ws_view_direction);
    float dir_x = view_dir.x;
    float dir_z = view_dir.z;
    float rotation_angle = atan2(dir_z, dir_x);

    matrix4_t rot_matrix = glm::rotate(-rotation_angle, vector3_t(0.0f, 1.0f, 0.0f));

    p->render->render_data.model = glm::translate(p->ws_position) * normal_rotation_matrix * glm::scale(player_scale);
    submit_skeletal_mesh(
        render_command_buffer,
        &player_mesh,
        &player_shader,
        &p->render->render_data,
        &p->animations);
}

static void s_render_ball(
    VkCommandBuffer render_command_buffer,
    player_t *p) {
    p->render->render_data.model = glm::translate(p->ws_position) * glm::scale(player_scale);
    submit_mesh(
        render_command_buffer,
        &player_ball_mesh,
        &player_ball_shader,
        &p->render->render_data);
}

// When skeletal animation is implemented, this function will do stuff like handle that
void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    struct world_t *world) {
    (void)transfer_command_buffer;
    
#if 0
    interpolate_joints(&test_instance0, logic_delta_time());
    sync_gpu_with_animated_transforms(&test_instance0, transfer_command_buffer);

    mesh_render_data_t render_data = {};
    render_data.model = glm::translate(glm::vec3(0.0f)) * glm::scale(player_scale);
    render_data.color = vector4_t(1.0f);
    render_data.pbr_info.x = 0.1f;
    render_data.pbr_info.y = 0.1f;
    submit_skeletal_mesh(
        render_command_buffer,
        &player_mesh,
        &player_shader,
        &render_data,
        &test_instance0);

    render_data.model = glm::translate(glm::vec3(5.0f, 0.0f, 0.0f)) * glm::scale(player_scale);
    
    submit_mesh(
        render_command_buffer,
        &player_ball_mesh,
        &player_ball_shader,
        &render_data);
#endif

#if 1
    for (uint32_t i = 0; i < world->players.data_count; ++i) {
        player_t *p = world->players[i];
        if (p) {
            if (p->flags.alive_state == PAS_ALIVE) {
                if (!p->render) {
                    w_player_render_init(p);
                }

                p->render->render_data.color = vector4_t(1.0f);
                p->render->render_data.pbr_info.x = 0.1f;
                p->render->render_data.pbr_info.y = 0.1f;

                if ((int32_t)i == (int32_t)world->local_player) {
                    if (p->flags.interaction_mode == PIM_STANDING) {
                        s_render_person(render_command_buffer, transfer_command_buffer, p);
                    }
                    else {
                        s_render_ball(render_command_buffer, p);
                    } 
                    // TODO: Special handling for first person mode
                }
                else {
                    if (p->flags.interaction_mode == PIM_STANDING) {
                        s_render_person(render_command_buffer, transfer_command_buffer, p);
                    }
                    else {
                        s_render_ball(render_command_buffer, p);
                    } 
                }
            }
        }
    }
#endif
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

player_t *w_add_player_from_info(
    player_init_info_t *init_info,
    world_t *world) {
    player_t *p = w_add_player(world);
        
    if (init_info->client_data) {
        p->name = init_info->client_data->name;
        p->client_id = init_info->client_data->client_id;
        // Now the network module can use w_get_player_from_client_id to get access to player directly
        w_link_client_id_to_local_id(p->client_id, p->local_id, world);
    }

    p->ws_position = init_info->ws_position;
    p->ws_view_direction = init_info->ws_view_direction;
    p->ws_up_vector = init_info->ws_up_vector;
    p->player_action_count = 0;
    p->default_speed = init_info->default_speed;
    p->next_random_spawn_position = init_info->next_random_spawn_position;
    p->ball_speed = 0.0f;
    p->ws_velocity = vector3_t(0.0f);
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
            w_set_local_player(p->local_id, world);
        }
        else {
            w_set_local_player(-1, world);
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
        w_player_animation_init(p);
    }

    LOG_INFOV("Added player %i: %s\n", p->local_id, p->name);
    return p;
}

void w_begin_ai_training_players(
    world_t *world,
    ai_training_session_t type) {
    world->spectator->ws_position = vector3_t(0.0f, -4.0f, 0.0f);

    switch (type) {

    case ATS_WALKING: {
        player_init_info_t info = {};
        info.ws_position = vector3_t(0.0f);
        info.ws_view_direction = vector3_t(0.0f, 0.0f, -1.0f);
        info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
        info.default_speed = 10.0f;

        player_flags_t flags = {};
        flags.u32 = 0;
        flags.alive_state = PAS_ALIVE;
        flags.interaction_mode = PIM_STANDING;

        info.flags = flags.u32;

        player_t *p = w_add_player_from_info(
            &info,
            world);

        uint32_t ai_id = attach_ai(p->local_id, 0);
    } break;

    }
}
