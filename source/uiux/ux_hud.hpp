#pragma once

#include <vkph_state.hpp>
#include <app.hpp>
#include <stdint.h>

namespace ux {

/*
  HUD will be for anything that displays on the screen while in-game.
  This could be the crosshair, health, map edtior color tables, etc...
*/
void init_hud();
void submit_hud(const vkph::state_t *state);
void hud_input(const app::raw_input_t *input);

/* 
  Gets rendered with the HUD
  Little buffer in corner of the screen to display the command that is being typed
  (Doesn't serve functionality - just a tool to help user see what is being typed).
*/
void begin_minibuffer();
void end_minibuffer();
void minibuffer_update_text(const char *text);
void submit_minibuffer();

// Display health, ammo, etc...
void begin_gameplay_display();
void end_gameplay_display();

// Color table
void color_table_init();
void begin_color_table();
void end_color_table();
void submit_color_table();

}
