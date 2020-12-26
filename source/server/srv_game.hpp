#pragma once

#include <vkph_state.hpp>

void srv_game_init(vkph::state_t *state);
void srv_game_tick(vkph::state_t *state);
void spawn_player(uint32_t client_id, vkph::state_t *state);
