#pragma once

#include <app.hpp>
#include <vkph_state.hpp>

void ui_main_menu_init(const vkph::state_t *state);
void ui_submit_main_menu();
void ui_main_menu_input(const app::raw_input_t *input);
void ui_clear_main_menu();
void ui_refresh_main_menu_server_page();
