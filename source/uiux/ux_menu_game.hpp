#pragma once

#include <app.hpp>
#include <vkph_state.hpp>

namespace ux {

void init_game_menu();
void init_game_menu_for_server(const vkph::state_t *state);
void submit_game_menu();
void game_menu_input(const app::raw_input_t *input, vkph::state_t *state);

enum play_button_function_t { PBF_SPAWN, PBF_UNPAUSE };

void set_play_button_function(play_button_function_t function);

/*
  Spawn button will be locked when player is alive.
 */
void lock_spawn_button();
void unlock_spawn_button();

}
