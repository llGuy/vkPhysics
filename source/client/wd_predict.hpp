#pragma once

#include <common/tools.hpp>

void wd_set_local_player(int32_t id);
int32_t wd_get_local_player();
void wd_handle_local_player_input(float dt);
void wd_execute_player_actions(struct player_t *player, struct event_submissions_t *events);
// Predict the game state given the actions of the local player
void wd_predict_state(struct event_submissions_t *events);
void wd_kill_local_player(struct event_submissions_t *events);
struct terraform_package_t *wd_get_local_terraform_package();
