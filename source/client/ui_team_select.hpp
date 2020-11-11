#pragma once

void ui_team_select_init(struct menu_layout_t *layout);
void ui_team_select_input(
    struct raw_input_t *raw_input,
    struct event_submissions_t *events);
// If there are new teams / when joining a different game
void ui_update_team_roster_layout(menu_layout_t *layout);
// When players get added to various teams, need to update player count display
void ui_update_team_roster_display_text(menu_layout_t *layout);
void ui_submit_team_select();
