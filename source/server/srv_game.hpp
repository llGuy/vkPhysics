#pragma once

#include <vkph_state.hpp>

namespace srv {

void init_game(vkph::state_t *state);
void tick_game(vkph::state_t *state);
void spawn_player(uint32_t client_id, vkph::state_t *state);

}
