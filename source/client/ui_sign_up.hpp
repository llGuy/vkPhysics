#pragma once

#include <app.hpp>
#include <net_meta.hpp>

void ui_sign_up_menu_init();
void ui_submit_sign_up_menu();
void ui_sign_up_menu_input(const app::raw_input_t *input);
void ui_handle_sign_up_failed(net::request_error_t error_type);
