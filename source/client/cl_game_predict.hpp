#pragma once

#include <vkph_state.hpp>
#include <vkph_terraform.hpp>

namespace cl {

void set_local_player(int32_t id, vkph::state_t *state);
int32_t get_local_player(vkph::state_t *state);
void handle_local_player_input(float dt, vkph::state_t *state);
void execute_player_actions(vkph::player_t *player, vkph::state_t *state);
// Predict the game state given the actions of the local player
void predict_state(vkph::state_t *state);
void kill_local_player(vkph::state_t *state);
vkph::terraform_package_t *get_local_terraform_package();

void add_predicted_projectile_hit(vkph::player_t *hit_player, vkph::state_t *state);

}
