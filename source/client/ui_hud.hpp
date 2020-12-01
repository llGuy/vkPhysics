#pragma once 

#include <stdint.h>

// HUD with crosshair, etc...
void ui_hud_init();
void ui_submit_hud();
void ui_hud_input(
    struct event_submissions_t *events,
    struct raw_input_t *input);

// Gets rendered with the HUD
// Little buffer in corner of the screen to display the command that is being typed
// (Doesn't serve functionality - just tool to help user see what is being typed)
void ui_begin_minibuffer();
void ui_end_minibuffer();
void ui_minibuffer_update_text(const char *text);
void ui_submit_minibuffer();

// Display health, ammo, etc...
void ui_begin_gameplay_display();
void ui_end_gameplay_display();

// Color table
void ui_color_table_init();
void ui_begin_color_table();
void ui_end_color_table();
void ui_submit_color_table();
