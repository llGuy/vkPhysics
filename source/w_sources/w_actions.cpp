#include "common/player.hpp"
#include "world.hpp"
#include "net.hpp"
#include "w_internal.hpp"
#include <glm/gtx/projection.hpp>

static void s_handle_local_player(
    player_t *player,
    player_action_t *action,
    event_submissions_t *events) {
    int32_t local_player_index = w_local_player_index();

    // If this is local player, need to cache these commands to later send to server
    player->camera_distance.animate(action->dt);
    player->camera_fov.animate(action->dt);

    *w_get_local_current_terraform_package() = player->terraform_package;
    if (player->flags.interaction_mode != PIM_STANDING) {
        w_get_local_current_terraform_package()->ray_hit_terrain = 0;
    }

    vector3_t up_diff = player->next_camera_up - player->current_camera_up;
    if (glm::dot(up_diff, up_diff) > 0.00001f) {
        player->current_camera_up = glm::normalize(player->current_camera_up + up_diff * action->dt * 1.5f);
    }

    if (connected_to_server()) {
        if (player->cached_player_action_count < PLAYER_MAX_ACTIONS_COUNT * 2) {
            player->cached_player_actions[player->cached_player_action_count++] = *action;
        }
        else {
            LOG_WARNING("Too many cached player actions\n");
        }
    }

    if (player->flags.alive_state == PAS_DEAD) {
        submit_event(ET_LAUNCH_GAME_MENU_SCREEN, NULL, events);
        submit_event(ET_ENTER_SERVER_META_MENU, NULL, events);
    }
}

void w_execute_player_actions(
    player_t *player,
    event_submissions_t *events) {
    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        player_action_t *action = &player->player_actions[i];

        execute_action(player, action);

        int32_t local_player_index = w_local_player_index();

        if ((int32_t)player->local_id == local_player_index && connected_to_server())
            s_handle_local_player(player, action, events);

        if (player->flags.alive_state == PAS_DEAD)
            break;
    }

    player->player_action_count = 0;
}
