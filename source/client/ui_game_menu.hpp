#pragma once 

void ui_game_menu_init();
void ui_submit_game_menu();
void ui_game_menu_input(struct event_submissions_t *events, struct raw_input_t *input);
enum play_button_function_t { PBF_SPAWN, PBF_UNPAUSE };
void ui_set_play_button_function(play_button_function_t function);
