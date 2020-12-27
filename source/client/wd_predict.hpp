#pragma once

#include <common/tools.hpp>
#include <vkph_terraform.hpp>

void wd_set_local_player(int32_t id);
int32_t wd_get_local_player();
void wd_handle_local_player_input(float dt);
void wd_execute_player_actions(struct player_t *player);
// Predict the game state given the actions of the local player
void wd_predict_state();
void wd_kill_local_player();
vkph::terraform_package_t *wd_get_local_terraform_package();

void wd_add_predicted_projectile_hit(player_t *hit_player);
