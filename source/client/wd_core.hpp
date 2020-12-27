#pragma once

#include "cl_view.hpp"
#include <vkph_state.hpp>

// World initialise
void wd_init(vkph::state_t *state);
void wd_destroy();
void wd_game_input(float dt);
void wd_tick(vkph::state_t *state);
void wd_set_i_am_in_server(bool b);
bool wd_am_i_in_server();
void wd_clear_world();
