#pragma once

#include <tools.hpp>
#include <vkph_terraform.hpp>
#include <vkph_player.hpp>

void wd_set_local_player(int32_t id);
int32_t wd_get_local_player();
void wd_handle_local_player_input(float dt, vkph::state_t *state);
void wd_execute_player_actions(vkph::player_t *player, vkph::state_t *state);
// Predict the game state given the actions of the local player
void wd_predict_state(vkph::state_t *state);
void wd_kill_local_player(vkph::state_t *state);
vkph::terraform_package_t *wd_get_local_terraform_package();

void wd_add_predicted_projectile_hit(vkph::player_t *hit_player, vkph::state_t *state);
