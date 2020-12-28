#pragma once

#include <app.hpp>
#include <vkph_state.hpp>

void ui_team_select_init(struct menu_layout_t *layout);
void ui_team_select_input(
    const app::raw_input_t *raw_input,
    vkph::state_t *state);
// If there are new teams / when joining a different game
void ui_update_team_roster_layout(menu_layout_t *layout, const vkph::state_t *state);
// When players get added to various teams, need to update player count display
void ui_update_team_roster_display_text(menu_layout_t *layout, const vkph::state_t *state);
void ui_submit_team_select();
