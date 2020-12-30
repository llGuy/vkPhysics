#pragma once

#include <app.hpp>
#include <vkph_state.hpp>

#include "ux_menu_layout.hpp"

namespace ux {

void init_team_select_page(menu_layout_t *layout);
void team_select_page_input(
    const app::raw_input_t *raw_input,
    vkph::state_t *state);
// If there are new teams / when joining a different game
void update_team_roster_layout(menu_layout_t *layout, const vkph::state_t *state);
// When players get added to various teams, need to update player count display
void update_team_roster_display_text(menu_layout_t *layout, const vkph::state_t *state);
void submit_team_select_page();

}
