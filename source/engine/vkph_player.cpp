#include "vkph_chunk.hpp"
#include "vkph_state.hpp"
#include "vkph_player.hpp"
#include "vkph_physics.hpp"

namespace vkph {

void player_t::init_default_values(player_init_info_t *info) {
    ws_position = info->ws_position;
    ws_view_direction = info->ws_view_direction;
    ws_up_vector = info->ws_up_vector;
    player_action_count = 0;
    default_speed = info->default_speed;
    next_random_spawn_position = info->next_random_spawn_position;
    ball_speed = 0.0f;
    ws_velocity = vector3_t(0.0f);
    idx_in_chunk_list = -1;
    health = 200;
    snapshot_before = snapshot_before = 0;

    // FOR NOW: just have the default weapon setup

    selected_weapon = 0;
    init_weapons();
}

void player_t::init_weapons() {
    // TODO: Configure weapons based on selected class.
    weapon_count = 2;
    weapons[0].init(40, 40, firing_type_t::SEMI_AUTOMATIC, bullet_type_t::PROJECTILE, weapon_type_t::ROCKS, 5.0f);
    weapons[1].init(0, 0, firing_type_t::INVALID, bullet_type_t::INVALID, weapon_type_t::TERRAFORMER, 0);
}

void player_t::init(player_init_info_t *info, int16_t *client_to_local_id_map) {
    if (info->client_name) {
        name = info->client_name;
        client_id = info->client_id;
        client_to_local_id_map[info->client_id] = local_id;
    }

    init_default_values(info);

    memset(player_actions, 0, sizeof(player_actions));
    accumulated_dt = 0.0f;

    flags.u32 = info->flags;
}

void player_t::make_player_init_info(player_init_info_t *dst) const {
    dst->client_name = name;
    dst->client_id = client_id;
    dst->ws_position = ws_position;
    dst->ws_view_direction = ws_view_direction;
    dst->ws_up_vector = ws_up_vector;
    dst->next_random_spawn_position = next_random_spawn_position;
    dst->default_speed = default_speed;
    dst->flags = flags.u32;
}

void player_t::push_actions(player_action_t *action, bool override_adt) {
    if (player_action_count < PLAYER_MAX_ACTIONS_COUNT) {
        if (override_adt) {
            player_actions[player_action_count++] = *action;
        }
        else {
            accumulated_dt += action->dt;

            float max_change = 1.0f * accumulated_dt * PLAYER_TERRAFORMING_SPEED;
            int32_t max_change_i = (int32_t)max_change;

            if (max_change_i < 2) {
                action->accumulated_dt = 0.0f;
            }
            else {
                action->accumulated_dt = accumulated_dt;
                accumulated_dt = 0.0f;
            }

            player_actions[player_action_count++] = *action;
        }
    }
    else {
        // ...
        LOG_WARNING("Too many player actions\n");
    }
}

void player_t::execute_action(player_action_t *action, state_t *state) {
    // Shape switching can happen regardless of the interaction mode
    handle_shape_switch(action->switch_shapes, action->dt);

    switch (flags.interaction_mode) {
            
    case PIM_METEORITE: {
        execute_player_direction_change(action);
        accelerate_meteorite_player(action, state);
    } break;

        // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
    case PIM_STANDING: {
        execute_player_triggers(action, state);
        execute_player_direction_change(action);
        execute_standing_player_movement(action, state);

        check_player_dead(action, state);
    } break;

    case PIM_BALL: {
        execute_player_direction_change(action);
        execute_ball_player_movement(action, state);

        check_player_dead(action, state);
    } break;

    case PIM_FLOATING: {
        execute_player_triggers(action, state);
        execute_player_direction_change(action);
        execute_player_floating_movement(action);
    } break;

    default: {
    } break;

    }

    for (uint32_t i = 0; i < weapon_count; ++i) {
        weapons[i].elapsed += action->dt;
    }

    update_player_chunk_status(state);
}

vector3_t player_t::compute_view_position() {
    return ws_position + ws_up_vector * PLAYER_SCALE * 2.0f;
}

void player_t::handle_shape_switch(bool switch_shapes, float dt) {
    if (switch_shapes) {
        // If already switching shapes
        if (switching_shapes)
            shape_switching_time = SHAPE_SWITCH_ANIMATION_TIME - shape_switching_time;
        else
            shape_switching_time = 0.0f;

        if (flags.interaction_mode == PIM_STANDING) {
            flags.interaction_mode = PIM_BALL;
            switching_shapes = 1;
        }
        else if (flags.interaction_mode == PIM_BALL) {
            flags.interaction_mode = PIM_STANDING;
            switching_shapes = 1;

            // Need to set animation to the "STOP" animation if the velocity exceeds a certain value
            if (frame_displacement / dt > 4.0f)
                animated_state = PAS_STOP_FAST;
        }
    }

    if (switching_shapes)
        shape_switching_time += dt;

    if (shape_switching_time > PLAYER_SHAPE_SWITCH_DURATION) {
        shape_switching_time = 0.0f;
        switching_shapes = 0;
    }
}

void player_t::handle_weapon_switch(player_action_t *actions) {
    if (actions->switch_weapons) {
        if (actions->next_weapon == 0b111) {
            // Just cycle through to next weapon
            selected_weapon = (selected_weapon + 1) % weapon_count;
            LOG_INFOV("Player chose weapon: %d\n", selected_weapon);
        }
        else if (actions->next_weapon < weapon_count) {
            selected_weapon = actions->next_weapon;
            LOG_INFOV("Player chose weapon: %d\n", selected_weapon);
        }
    }
}

void player_t::execute_player_triggers(player_action_t *player_actions, state_t *state) {
    handle_weapon_switch(player_actions);

    weapon_t *weapon = &weapons[selected_weapon];

    switch (weapon->type) {
    case weapon_type_t::ROCKS: {
        // Spawn rock
        if (player_actions->trigger_left && weapon->elapsed > weapon->recoil_time) {
            weapon->elapsed = 0.0f;
            
            // TODO: Do check to see if the player can shoot...
            uint32_t ref_idx = weapon->active_projs.add();

            /*
              Spawn rock in the GAME STATE.
             */
            uint32_t rock_idx = state->rocks.spawn(
                compute_view_position(),
                ws_view_direction * PROJECTILE_ROCK_SPEED,
                ws_up_vector,
                client_id,
                ref_idx,
                selected_weapon);

            weapon->active_projs[ref_idx].initialised = 1;
            weapon->active_projs[ref_idx].idx = rock_idx;
        }

        terraform_package.ray_hit_terrain = 0;
    } break;

        // Special case
    case weapon_type_t::TERRAFORMER: {
        terraform_package = cast_terrain_ray(
            compute_view_position(),
            ws_view_direction,
            10.0f,
            terraform_package.color,
            state);

        if (player_actions->trigger_left) {
            terraform_info_t info = {};
            info.dt = player_actions->accumulated_dt;
            info.package = &terraform_package;
            info.radius = PLAYER_TERRAFORMING_RADIUS;
            info.speed = PLAYER_TERRAFORMING_SPEED;
            info.type = TT_DESTROY;

            state->terraform(&info);
        }
        if (player_actions->trigger_right) {
            terraform_info_t info = {};
            info.dt = player_actions->accumulated_dt;
            info.package = &terraform_package;
            info.radius = PLAYER_TERRAFORMING_RADIUS;
            info.speed = PLAYER_TERRAFORMING_SPEED;
            info.type = TT_BUILD;

            state->terraform(&info);
        }

        if (player_actions->flashlight) {
            flags.flashing_light ^= 1;
        }
    } break;
    }
}

void player_t::accelerate_meteorite_player(player_action_t *actions, const state_t *state) {
    // Need to set player's up vector depending on direction it is flying
    vector3_t right = glm::normalize(glm::cross(ws_view_direction, ws_up_vector));
    ws_up_vector = glm::normalize(glm::cross(right, ws_view_direction));

    next_camera_up = ws_up_vector;

    static const float METEORITE_ACCELERATION = 25.0f;
    static const float MAX_METEORITE_SPEED = 25.0f;

    // Apply air resistance
    ws_velocity -= (ws_velocity * 0.1f) * actions->dt;

    vector3_t final_velocity = ws_velocity * actions->dt;

    vector3_t player_scale = vector3_t(PLAYER_SCALE);

    terrain_collision_t collision = {};
    collision.ws_size = player_scale;
    collision.ws_position = ws_position;
    collision.ws_velocity = final_velocity;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    ws_position = collide_and_slide(&collision, state) * player_scale;
    ws_velocity = (collision.es_velocity * player_scale) / actions->dt;

    if (collision.detected) {
        // Change interaction mode to ball
        flags.interaction_mode = PIM_BALL;
        camera_distance.set(1, 10.0f, camera_distance.current, 1.0f);
        camera_fov.set(1, 60.0f, camera_fov.current);

        vector3_t normal = glm::normalize(collision.es_surface_normal * player_scale);
        
        next_camera_up = normal;
        ws_up_vector = normal;

        flags.is_on_ground = 1;
    }
    else {
        flags.is_on_ground = 0;

        if (glm::dot(ws_velocity, ws_velocity) < MAX_METEORITE_SPEED * MAX_METEORITE_SPEED) {
            //meteorite_speed += METEORITE_ACCELERATION * actions->dt;
            ws_velocity += ws_view_direction * METEORITE_ACCELERATION * actions->dt;
        }
        else {
            ws_velocity = glm::normalize(ws_velocity) * MAX_METEORITE_SPEED;
        }
    }
}

void player_t::execute_player_direction_change(player_action_t *player_actions) {
    vector2_t delta = vector2_t(player_actions->dmouse_x, player_actions->dmouse_y);

    static constexpr float SENSITIVITY = 15.0f;

    vector3_t res = ws_view_direction;

    float x_angle = glm::radians(-delta.x) * SENSITIVITY * player_actions->dt;
    float y_angle = glm::radians(-delta.y) * SENSITIVITY * player_actions->dt;
                
    res = matrix3_t(glm::rotate(x_angle, ws_up_vector)) * res;
    vector3_t rotate_y = glm::cross(res, ws_up_vector);
    res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

    res = glm::normalize(res);
                
    ws_view_direction = res;
}

void player_t::execute_player_floating_movement(player_action_t *actions) {
    vector3_t right = glm::normalize(glm::cross(ws_view_direction, ws_up_vector));
    vector3_t forward = glm::normalize(glm::cross(ws_up_vector, right));

    if (actions->move_forward)
        ws_position += forward * actions->dt * default_speed;
    if (actions->move_left)
        ws_position -= right * actions->dt * default_speed;
    if (actions->move_back)
        ws_position -= forward * actions->dt * default_speed;
    if (actions->move_right)
        ws_position += right * actions->dt * default_speed;
    if (actions->jump)
        ws_position += ws_up_vector * actions->dt * default_speed;
    if (actions->crouch)
        ws_position -= ws_up_vector * actions->dt * default_speed;
}

void player_t::execute_standing_player_movement(player_action_t *actions, const state_t *state) {
    if (flags.is_on_ground) {
        if (animated_state == PAS_STOP_FAST && frame_displacement / actions->dt > 0.002f) {
            // Need to slow down
            actions->move_back = 0;
            actions->move_forward = 0;
            actions->move_left = 0;
            actions->move_right = 0;
            actions->jump = 0;
        }
        else if (actions->move_forward) {
            animated_state = PAS_RUNNING;
        }
        else if (actions->move_back) {
            animated_state = PAS_BACKWALKING;
        }
        else if (actions->move_left) {
            animated_state = PAS_LEFT_WALKING;
        }
        else if (actions->move_right) {
            animated_state = PAS_RIGHT_WALKING;
        }
        else {
            animated_state = PAS_IDLE;
        }

        if (actions->jump) {
            ws_velocity += ws_up_vector * 4.0f;

            animated_state = PAS_JUMPING_UP;

            flags.is_on_ground = 0;
        }
    }

    force_values_t forces = {};
    forces.friction = 5.0f;
    forces.movement_acceleration = 8.5f;
    forces.gravity = GRAVITY_ACCELERATION;
    forces.maximum_walking_speed = default_speed * 0.8f;

    resolve_player_movement(
        this,
        actions,
        &forces,
        MRF_GRAVITY_CHECK_INCLINATION | MRF_ABRUPT_STOP | MRF_CAP_SPEED,
        state);
}

void player_t::execute_ball_player_movement(player_action_t *actions, const state_t *state) {
    force_values_t forces = {};
    forces.friction = 1.0f;
    forces.movement_acceleration = 10.0f;
    forces.gravity = 10.0f;
    forces.maximum_walking_speed = default_speed;
    resolve_player_movement(this, actions, &forces, MRF_ADOPT_GRAVITY | MRF_CAP_SPEED, state);
}

void player_t::check_player_dead(player_action_t *actions, state_t *state) {
    // Start checking for death, if the velocity vector
    // is pretty aligned with the up vector (but down) of the player
    // because if it is, that means that the velocity vector won't be able to change too much.
    // (gravity has already pulled the player out of any sort of movement range)
    if (!flags.is_on_ground) {
        vector3_t normalized_velocity = glm::normalize(ws_velocity);

        float vdu = glm::dot(normalized_velocity, glm::normalize(-ws_up_vector));

        if (vdu > 0.9f) {
            // Do ray cast to check if there are chunks underneath the player
            uint32_t ray_step_count = 10;
            vector3_t current_ray_position = ws_position;
            current_ray_position += 16.0f * normalized_velocity;
            for (uint32_t i = 0; i < ray_step_count; ++i) {
                ivector3_t chunk_coord = space_voxel_to_chunk(space_world_to_voxel(current_ray_position));

                chunk_t *c = state->access_chunk(chunk_coord);
                if (c) {
                    // Might not die, reset timer
                    death_checker = 0.0f;

                    break;
                }

                current_ray_position += 16.0f * normalized_velocity;
            }

            death_checker += actions->dt;

            if (death_checker > 5.0f) {
                // The player is dead
                LOG_INFO("Player has just died\n");

                flags.is_alive = 0;
                frame_displacement = 0.0f;
            }
        }
    }
    else {
        death_checker = 0.0f;
    }
}

void player_t::update_player_chunk_status(state_t *state) {
    // If the player has been added to a chunk that hadn't been created at the time
    if (idx_in_chunk_list == -1) {
        chunk_t *c = state->access_chunk(chunk_coord);

        if (c) {
            uint32_t idx = c->players_in_chunk.add();
            c->players_in_chunk[idx] = local_id;
            idx_in_chunk_list = idx;
        }
    }

    // Update the chunk in which the player is in
    ivector3_t new_chunk_coord = space_voxel_to_chunk(space_world_to_voxel(ws_position));

    if (
        new_chunk_coord.x != chunk_coord.x ||
        new_chunk_coord.y != chunk_coord.y ||
        new_chunk_coord.z != chunk_coord.z) {
        // Chunk is different

        // Remove player from previous chunk
        if (idx_in_chunk_list != -1) {
            chunk_t *c = state->access_chunk(chunk_coord);
            c->players_in_chunk.remove(idx_in_chunk_list);

            idx_in_chunk_list = -1;
        }

        chunk_coord = new_chunk_coord;

        chunk_t *c = state->access_chunk(new_chunk_coord);

        if (c) {
            uint32_t idx = c->players_in_chunk.add();
            c->players_in_chunk[idx] = local_id;
            idx_in_chunk_list = idx;
        }
    }
}

}
