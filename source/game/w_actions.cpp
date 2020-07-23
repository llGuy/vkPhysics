#include "game/world.hpp"
#include "net.hpp"
#include "w_internal.hpp"
#include <glm/gtx/projection.hpp>

static void s_execute_player_triggers(
    player_t *player,
    player_actions_t *player_actions) {
    player->terraform_package = w_cast_terrain_ray(
        player->ws_position,
        player->ws_view_direction,
        10.0f);

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
            player_actions->accumulated_dt);
    }
    if (player_actions->trigger_right) {
        w_terraform(
            TT_BUILD,
            player->terraform_package,
            TERRAFORMING_RADIUS,
            TERRAFORMING_SPEED,
            player_actions->accumulated_dt);
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

    if (actions->move_forward)
        player->ws_position += forward * actions->dt * player->default_speed;
    if (actions->move_left)
        player->ws_position -= right * actions->dt * player->default_speed;
    if (actions->move_back)
        player->ws_position -= forward * actions->dt * player->default_speed;
    if (actions->move_right)
        player->ws_position += right * actions->dt * player->default_speed;
    if (actions->jump)
        player->ws_position += player->ws_up_vector * actions->dt * player->default_speed;
    if (actions->crouch)
        player->ws_position -= player->ws_up_vector * actions->dt * player->default_speed;
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
    collision.ws_size = w_get_player_scale();
    collision.ws_position = player->ws_position;
    collision.ws_velocity = player->ws_velocity * actions->dt;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    vector3_t ws_new_position = w_collide_and_slide(&collision) * w_get_player_scale();

    if (player->flags.contact != PCS_IN_AIR) {
        float len = glm::length(ws_new_position - player->ws_position);
        if (len < 0.00001f) {
            player->frame_displacement = 0.0f;
        }
        else {
            player->frame_displacement = len;
        }
    }

    player->ws_position = ws_new_position;

    if (collision.detected) {
        vector3_t normal = glm::normalize(collision.es_surface_normal * w_get_player_scale());
        player->ws_surface_normal = normal;

        if (player->flags.contact == PCS_IN_AIR) {
            if (glm::abs(glm::dot(player->ws_velocity, player->ws_velocity)) == 0.0f) {
                float previous_velocity_length = glm::length(player->ws_velocity);
                movement_axes_t new_axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);
                player->ws_velocity = glm::normalize(glm::proj(player->ws_velocity, new_axes.forward)) * previous_velocity_length * 0.2f;
            }
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
        player->flags.contact = PCS_IN_AIR;
    }
}

static void s_execute_standing_player_movement(
    player_t *player,
    player_actions_t *actions) {
    if (player->flags.contact == PCS_ON_GROUND) {
        if (player->animated_state == PAS_TRIPPING && player->frame_displacement / actions->dt > 0.5f) {
            // Need to slow down
        }
        else if (actions->move_forward) {
            player->animated_state = PAS_RUNNING;
        }
        else if (actions->move_back) {
            player->animated_state = PAS_BACKWALKING;
        }
        else if (actions->move_left) {
            player->animated_state = PAS_LEFT_WALKING;
        }
        else if (actions->move_right) {
            player->animated_state = PAS_RIGHT_WALKING;
        }
        else {
            player->animated_state = PAS_IDLE;
        }

        if (actions->jump) {
            player->ws_velocity += player->ws_up_vector * 4.0f;

            player->animated_state = PAS_JUMPING_UP;

            player->flags.contact = PCS_IN_AIR;
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

    vector3_t player_scale = w_get_player_scale();

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

                chunk_t *c = access_chunk(chunk_coord);
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

                if (player->flags.is_local) {
                    submit_event(ET_LAUNCH_GAME_MENU_SCREEN, NULL, events);
                    submit_event(ET_ENTER_SERVER_META_MENU, NULL, events);
                }
            }
        }
    }
    else {
        player->death_checker = 0.0f;
    }
}

static void s_handle_shape_switch(
    player_t *player,
    player_actions_t *actions) {
    if (actions->switch_shapes) {
        // If already switching shapes
        if (player->switching_shapes) {
            player->shape_animation_time = SHAPE_SWITCH_ANIMATION_TIME - player->shape_animation_time;
        }
        else {
            player->shape_animation_time = 0.0f;
        }

        if (player->flags.interaction_mode == PIM_STANDING) {
            player->flags.interaction_mode = PIM_BALL;
            player->switching_shapes = 1;
        }
        else if (player->flags.interaction_mode == PIM_BALL) {
            player->flags.interaction_mode = PIM_STANDING;
            player->switching_shapes = 1;

            // Need to set animation to the "STOP" animation if the velocity exceeds a certain value
            if (player->frame_displacement / actions->dt > 8.0f) {
                LOG_INFOV("Frame displacement at: %f\n", player->frame_displacement / actions->dt);

                player->animated_state = PAS_TRIPPING;
            }
        }
    }

    if (player->switching_shapes) {
        player->shape_animation_time += logic_delta_time();
    }

    if (player->shape_animation_time > SHAPE_SWITCH_ANIMATION_TIME) {
        player->shape_animation_time = 0.0f;
        player->switching_shapes = 0;
    }
}

static void s_handle_local_player(
    player_t *player,
    player_actions_t *actions) {
    int32_t local_player_index = w_local_player_index();

    // If this is local player, need to cache these commands to later send to server
    if ((int32_t)player->local_id == local_player_index && connected_to_server()) {
        player->camera_distance.animate(actions->dt);
        player->camera_fov.animate(actions->dt);

        *w_get_local_current_terraform_package() = player->terraform_package;
        if (player->flags.interaction_mode != PIM_STANDING) {
            w_get_local_current_terraform_package()->ray_hit_terrain = 0;
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
}

void w_execute_player_actions(
    player_t *player,
    event_submissions_t *events) {
    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        player_actions_t *actions = &player->player_actions[i];

        s_handle_shape_switch(player, actions);

        switch (player->flags.interaction_mode) {
            
        case PIM_METEORITE: {
            s_execute_player_direction_change(player, actions);
            s_accelerate_meteorite_player(player, actions);
        } break;

            // FOR NOW STANDING AND BALL ARE EQUIVALENT ///////////////////////
        case PIM_STANDING: {
            s_execute_player_triggers(player, actions);
            s_execute_player_direction_change(player, actions);
            s_execute_standing_player_movement(player, actions);

            s_check_player_dead(actions, player, events);
        } break;

        case PIM_BALL: {
            s_execute_player_direction_change(player, actions);
            s_execute_ball_player_movement(player, actions);

            s_check_player_dead(actions, player, events);
        } break;

        case PIM_FLOATING: {
            s_execute_player_triggers(player, actions);
            s_execute_player_direction_change(player, actions);
            s_execute_player_floating_movement(player, actions);
        } break;

        default: {
        } break;

        }

        s_handle_local_player(
            player,
            actions);

        if (player->flags.alive_state == PAS_DEAD) {
            break;
        }
    }

    player->player_action_count = 0;
}

void push_player_actions(
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
