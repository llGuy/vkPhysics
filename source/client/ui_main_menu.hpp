#pragma once

#include <app.hpp>

void ui_main_menu_init();
void ui_submit_main_menu();
void ui_main_menu_input(struct event_submissions_t *events, const app::raw_input_t *input);
void ui_clear_main_menu();
void ui_refresh_main_menu_server_page();
