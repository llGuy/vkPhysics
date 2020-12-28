#pragma once 

#include <app.hpp>
#include <vkph_state.hpp>

void ui_game_menu_init();
void ui_init_game_menu_for_server(const vkph::state_t *state);
void ui_submit_game_menu();
void ui_game_menu_input(const app::raw_input_t *input, vkph::state_t *state);
enum play_button_function_t { PBF_SPAWN, PBF_UNPAUSE };
void ui_set_play_button_function(play_button_function_t function);
void ui_lock_spawn_button();
void ui_unlock_spawn_button();
