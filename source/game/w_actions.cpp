#include "game/world.hpp"
#include "net.hpp"
#include "w_internal.hpp"
#include <glm/gtx/projection.hpp>



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
