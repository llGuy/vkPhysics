#pragma once

#include <app.hpp>
#include <net_meta.hpp>

namespace ux {

void init_sign_up_menu();
void submit_sign_up_menu();
void sign_up_menu_input(const app::raw_input_t *input);
void handle_sign_up_failed(net::request_error_t error_type);

}
