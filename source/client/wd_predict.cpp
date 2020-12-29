#include "nw_client.hpp"
#include "dr_player.hpp"
#include "wd_predict.hpp"
#include "wd_spectate.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_player.hpp>
#include <app.hpp>
#include <constant.hpp>
#include <net_context.hpp>

static int32_t local_player;

static vkph::terraform_package_t local_current_terraform_package;

static stack_container_t<vkph::predicted_projectile_hit_t> hits;

void wd_set_local_player(int32_t id) {
    local_player = id;
}

int32_t wd_get_local_player() {
    return local_player;
}

static vkph::player_t *s_get_local_player(vkph::state_t *state) {
    if (local_player >= 0) {
        return state->get_player(local_player);
    }
    else {
        return NULL;
    }
}

void wd_handle_local_player_input(float dt, vkph::state_t *state) {
    const app::game_input_t *game_input = app::get_game_input();

    vkph::player_action_t actions;
    memset(&actions, 0, sizeof(actions));

    actions.bytes = 0;

    actions.accumulated_dt = 0.0f;

    actions.dt = dt;
    actions.dmouse_x = game_input->mouse_x - game_input->previous_mouse_x;
    actions.dmouse_y = game_input->mouse_y - game_input->previous_mouse_y;

    if (game_input->actions[app::GIAT_MOVE_FORWARD].state == app::BS_DOWN)
        actions.move_forward = 1;
    if (game_input->actions[app::GIAT_MOVE_LEFT].state == app::BS_DOWN)
        actions.move_left = 1;
    if (game_input->actions[app::GIAT_MOVE_BACK].state == app::BS_DOWN)
        actions.move_back = 1;
    if (game_input->actions[app::GIAT_MOVE_RIGHT].state == app::BS_DOWN)
        actions.move_right = 1;
    if (game_input->actions[app::GIAT_TRIGGER4].state == app::BS_DOWN)
        actions.jump = 1;
    if (game_input->actions[app::GIAT_TRIGGER6].state == app::BS_DOWN)
        actions.crouch = 1;
    if (game_input->actions[app::GIAT_TRIGGER1].state == app::BS_DOWN)
        actions.trigger_left = 1;
    if (game_input->actions[app::GIAT_TRIGGER2].state == app::BS_DOWN)
        actions.trigger_right = 1;
    if (game_input->actions[app::GIAT_TRIGGER5].release == app::BS_DOWN)
        actions.switch_shapes = 1;
    if (game_input->actions[app::GIAT_TRIGGER7].instant == app::BS_DOWN)
        actions.flashlight = 1;

    if (game_input->actions[app::GIAT_TRIGGER8].instant == app::BS_DOWN) {
        actions.switch_weapons = 1;
        actions.next_weapon = 0b111;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        if (game_input->actions[app::GIAT_NUMBER0 + i].instant == app::BS_DOWN) {
            actions.switch_weapons = 1;
            actions.next_weapon = i;
        }
    }

    actions.tick = state->current_tick;
    
    vkph::player_t *local_player_ptr = s_get_local_player(state);

    if (local_player_ptr) {
        if (local_player_ptr->flags.is_alive) {
            local_player_ptr->push_actions(&actions, 0);
        }
        else {
            wd_get_spectator()->push_actions(&actions, 0);
        }
    }
    else {
        wd_get_spectator()->push_actions(&actions, 0);
    }
}

void wd_execute_player_actions(vkph::player_t *player, vkph::state_t *state) {
    for (uint32_t i = 0; i < player->player_action_count; ++i) {
        vkph::player_action_t *action = &player->player_actions[i];

        player->execute_action(action, state);

        if (nw_connected_to_server()) {
            // If this is local player, need to cache these commands to later send to server
            player->camera_distance.animate(action->dt);
            player->camera_fov.animate(action->dt);

            local_current_terraform_package = player->terraform_package;
            if (player->flags.interaction_mode != vkph::PIM_STANDING) {
                local_current_terraform_package.ray_hit_terrain = 0;
            }

            vector3_t up_diff = player->next_camera_up - player->current_camera_up;
            if (glm::dot(up_diff, up_diff) > 0.00001f) {
                player->current_camera_up = glm::normalize(player->current_camera_up + up_diff * action->dt * 1.5f);
            }

            if (nw_connected_to_server()) {
                if (player->cached_player_action_count < vkph::PLAYER_MAX_ACTIONS_COUNT * 2) {
                    player->cached_player_actions[player->cached_player_action_count++] = *action;
                }
                else {
                    LOG_WARNING("Too many cached player actions\n");
                }
            }

            if (!player->flags.is_alive) {
                player->render->rotation_speed = 0.0f;
                player->render->rotation_angle = 0.0f;
                player->render->rolling_matrix = matrix4_t(1.0f);
                vkph::submit_event(vkph::ET_LOCAL_PLAYER_DIED, NULL);
            }
        }

        if (!player->flags.is_alive)
            break;
    }

    player->player_action_count = 0;
}

void wd_predict_state(vkph::state_t *state) {
    vkph::player_t *player = s_get_local_player(state);
    
    if (player) {
        wd_execute_player_actions(player, state);
    }
}

void wd_kill_local_player(vkph::state_t *state) {
    LOG_INFO("Local player just died\n");

    vkph::submit_event(vkph::ET_LOCAL_PLAYER_DIED, NULL);

    vkph::player_t *local_player_ptr = s_get_local_player(state);
    local_player_ptr->render->rotation_speed = 0.0f;
    local_player_ptr->render->rotation_angle = 0.0f;
    local_player_ptr->render->rolling_matrix = matrix4_t(1.0f);
    local_player_ptr->flags.is_alive = false;
}

vkph::terraform_package_t *wd_get_local_terraform_package() {
    return &local_current_terraform_package;
}

void wd_add_predicted_projectile_hit(vkph::player_t *hit_player, vkph::state_t *state) {
    vkph::predicted_projectile_hit_t new_hit = {};
    new_hit.flags.initialised = 1;
    new_hit.client_id = hit_player->client_id;
    new_hit.progression = hit_player->elapsed / net::NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL;

    vkph::player_snapshot_t *before = &hit_player->remote_snapshots.buffer[hit_player->snapshot_before];
    vkph::player_snapshot_t *after = &hit_player->remote_snapshots.buffer[hit_player->snapshot_after];

    new_hit.tick_before = before->tick;
    new_hit.tick_after = after->tick;

    uint32_t hit_idx = state->predicted_hits.add();
    state->predicted_hits[hit_idx] = new_hit;
}
