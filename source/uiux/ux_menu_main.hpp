#pragma once

#include <app.hpp>
#include <vkph_state.hpp>

namespace ux {

void init_main_menu(const vkph::state_t *state);
void submit_main_menu();
void main_menu_input(const app::raw_input_t *input);
void clear_main_menu();
void refresh_main_menu_server_page();

}
