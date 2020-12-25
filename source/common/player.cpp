#include "game.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include "constant.hpp"
#include <glm/gtx/projection.hpp>
#include <glm/gtx/string_cast.hpp>


static void s_execute_player_direction_change(
    player_t *player,
    player_action_t *player_actions) {
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

static void s_execute_player_floating_movement(
    player_t *player,
    player_action_t *actions) {
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
    player_action_t *actions,
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
    player_action_t *actions,
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

static void s_resolve_player_movement(
    player_t *player,
    player_action_t *actions,
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
    collision.ws_size = vector3_t(PLAYER_SCALE);
    collision.ws_position = player->ws_position;
    collision.ws_velocity = player->ws_velocity * actions->dt;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    vector3_t ws_new_position = collide_and_slide(&collision) * PLAYER_SCALE;

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
        vector3_t normal = glm::normalize(collision.es_surface_normal * PLAYER_SCALE);
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
    player_action_t *actions) {
    if (player->flags.contact == PCS_ON_GROUND) {
        if (player->animated_state == PAS_STOP_FAST && player->frame_displacement / actions->dt > 0.002f) {
            // Need to slow down
            actions->move_back = 0;
            actions->move_forward = 0;
            actions->move_left = 0;
            actions->move_right = 0;
            actions->jump = 0;
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
    forces.friction = 5.0f;
    forces.movement_acceleration = 8.5f;
    forces.gravity = GRAVITY_ACCELERATION;
    forces.maximum_walking_speed = player->default_speed * 0.8;

    s_resolve_player_movement(
        player,
        actions,
        &forces,
        MRF_GRAVITY_CHECK_INCLINATION | MRF_ABRUPT_STOP | MRF_CAP_SPEED);
}

static void s_execute_ball_player_movement(
    player_t *player,
    player_action_t *actions) {
    force_values_t forces = {};
    forces.friction = 1.0f;
    forces.movement_acceleration = 10.0f;
    forces.gravity = 10.0f;
    forces.maximum_walking_speed = player->default_speed;
    s_resolve_player_movement(player, actions, &forces, MRF_ADOPT_GRAVITY | MRF_CAP_SPEED);
}

static void s_accelerate_meteorite_player(
    player_t *player,
    player_action_t *actions) {
    // Need to set player's up vector depending on direction it is flying
    vector3_t right = glm::normalize(glm::cross(player->ws_view_direction, player->ws_up_vector));
    player->ws_up_vector = glm::normalize(glm::cross(right, player->ws_view_direction));

    player->next_camera_up = player->ws_up_vector;

    static const float METEORITE_ACCELERATION = 25.0f;
    static const float MAX_METEORITE_SPEED = 25.0f;

    // Apply air resistance
    player->ws_velocity -= (player->ws_velocity * 0.1f) * actions->dt;

    vector3_t final_velocity = player->ws_velocity * actions->dt;

    vector3_t player_scale = vector3_t(PLAYER_SCALE);

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
    player_action_t *actions,
    player_t *player) {
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
                ivector3_t chunk_coord = space_voxel_to_chunk(space_world_to_voxel(current_ray_position));

                chunk_t *c = g_game->access_chunk(chunk_coord);
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
                player->frame_displacement = 0.0f;
            }
        }
    }
    else {
        player->death_checker = 0.0f;
    }
}

void update_player_chunk_status(player_t *player) {
    // If the player has been added to a chunk that hadn't been created at the time
    if (player->idx_in_chunk_list == -1) {
        chunk_t *c = g_game->access_chunk(player->chunk_coord);

        if (c) {
            uint32_t idx = c->players_in_chunk.add();
            c->players_in_chunk[idx] = player->local_id;
            player->idx_in_chunk_list = idx;
        }
    }

    // Update the chunk in which the player is in
    ivector3_t new_chunk_coord = space_voxel_to_chunk(space_world_to_voxel(player->ws_position));

    if (
        new_chunk_coord.x != player->chunk_coord.x ||
        new_chunk_coord.y != player->chunk_coord.y ||
        new_chunk_coord.z != player->chunk_coord.z) {
        // Chunk is different

        // Remove player from previous chunk
        if (player->idx_in_chunk_list != -1) {
            chunk_t *c = g_game->access_chunk(player->chunk_coord);
            c->players_in_chunk.remove(player->idx_in_chunk_list);

            player->idx_in_chunk_list = -1;
        }

        player->chunk_coord = new_chunk_coord;

        chunk_t *c = g_game->access_chunk(new_chunk_coord);

        if (c) {
            uint32_t idx = c->players_in_chunk.add();
            c->players_in_chunk[idx] = player->local_id;
            player->idx_in_chunk_list = idx;
        }
    }
}

vector3_t compute_player_view_position(const player_t *p) {
    return p->ws_position + p->ws_up_vector * PLAYER_SCALE * 2.0f;
}

bool collide_sphere_with_standing_player(
    const vector3_t &target,
    const vector3_t &target_up,
    const vector3_t &center,
    float radius) {
    float player_height = PLAYER_SCALE * 2.0f;

    // Check collision with 2 spheres
    float sphere_scale = player_height * 0.5f;
    vector3_t body_low = target + (target_up * player_height * 0.22f);
    vector3_t body_high = target + (target_up * player_height * 0.75f);

    vector3_t body_low_diff = body_low - center;
    vector3_t body_high_diff = body_high - center;

    float dist2_low = glm::dot(body_low_diff, body_low_diff);
    float dist2_high = glm::dot(body_high_diff, body_high_diff);

    float dist_min = radius + sphere_scale;
    float dist_min2 = dist_min * dist_min;

    if (dist2_low < dist_min2 || dist2_high < dist_min2) {
        return true;
    }
    else {
        return false;
    }
}

bool collide_sphere_with_rolling_player(const vector3_t &target, const vector3_t &center, float radius) {
    float dist_min = radius + PLAYER_SCALE;
    float dist_min2 = dist_min * dist_min;

    vector3_t diff = target - center;
    float dist_to_player2 = glm::dot(diff, diff);

    if (dist_to_player2 < dist_min2) {
        return true;
    }
    else {
        return false;
    }
}

bool collide_sphere_with_player(
    const player_t *p,
    const vector3_t &center,
    float radius) {
    if (p->flags.interaction_mode == PIM_STANDING ||
        p->flags.interaction_mode == PIM_FLOATING) {
        return collide_sphere_with_standing_player(
            p->ws_position,
            p->ws_up_vector,
            center,
            radius);
    }
    else {
        return collide_sphere_with_rolling_player(
            p->ws_position,
            center,
            radius);
    }

    return false;
}
