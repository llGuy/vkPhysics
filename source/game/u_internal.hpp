#pragma once

#include "ui.hpp"
#include <common/math.hpp>

// GLOBAL STUFF ///////////////////////////////////////////////////////////////
struct font_t *u_game_font();

// MAIN MENU //////////////////////////////////////////////////////////////////
void u_main_menu_init();
void u_submit_main_menu();
void u_main_menu_input(struct event_submissions_t *events, struct raw_input_t *input);
void u_clear_main_menu();

// HUD ////////////////////////////////////////////////////////////////////////
void u_hud_init();
void u_submit_hud();

// GAME MENU //////////////////////////////////////////////////////////////////
void u_game_menu_init();
void u_submit_game_menu();
void u_game_menu_input(struct event_submissions_t *events, struct raw_input_t *input);

// UTILITIES //////////////////////////////////////////////////////////////////
// This allows for interpolation when user hovers / unhovers over widgets

struct color_pair_t {
    uint32_t current_foreground, current_background;
};

struct widget_color_t {
    uint32_t unhovered_background;
    uint32_t hovered_background;
    uint32_t unhovered_foreground;
    uint32_t hovered_foreground;

    bool is_hovered_on;

    smooth_linear_interpolation_v3_t background_color;
    smooth_linear_interpolation_v3_t foreground_color;

    void init(
        uint32_t unhovered_background,
        uint32_t hovered_background,
        uint32_t unhovered_foreground,
        uint32_t hovered_foreground);

    void start_interpolation(
        float interpolation_speed,
        uint32_t final_background,
        uint32_t final_foreground);

    color_pair_t update(
        float interpolation_speed,
        bool hovered_on);
};

bool u_hover_over_box(
    struct ui_box_t *box,
    float cursor_x,
    float cursor_y);
