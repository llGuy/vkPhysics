#pragma once

#include "ui.hpp"
#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>

// GLOBAL STUFF ///////////////////////////////////////////////////////////////
struct font_t *u_game_font();

enum ui_texture_t {
    UT_PLAY_ICON,
    UT_SETTINGS_ICON,
    UT_BUILD_ICON,
    UT_QUIT_ICON,
    UT_SPAWN_ICON,
    UT_CROSSHAIRS,
    UT_INVALID_TEXTURE
};

VkDescriptorSet u_texture(
    ui_texture_t type);

// MAIN MENU //////////////////////////////////////////////////////////////////
void u_main_menu_init();
void u_submit_main_menu();
void u_main_menu_input(struct event_submissions_t *events, struct raw_input_t *input);
void u_clear_main_menu();
void u_refresh_main_menu_server_page();

// HUD ////////////////////////////////////////////////////////////////////////
void u_hud_init();
void u_submit_hud();

// GAME MENU //////////////////////////////////////////////////////////////////
void u_game_menu_init();
void u_submit_game_menu();
void u_game_menu_input(struct event_submissions_t *events, struct raw_input_t *input);

// SIGN UP MENU ///////////////////////////////////////////////////////////////
void u_sign_up_menu_init();
void u_submit_sign_up_menu();
void u_sign_up_menu_input(struct event_submissions_t *events, struct raw_input_t *input);

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

struct menu_widget_t {
    texture_t texture;
    ui_box_t box;
    ui_box_t image_box;

    widget_color_t color;

    bool hovered_on;
};

#define MAX_MENU_WIDGETS 4

#define MENU_WIDGET_NOT_HOVERED_OVER_ICON_COLOR 0xFFFFFF36
#define MENU_WIDGET_NOT_HOVERED_OVER_BACKGROUND_COLOR 0x16161636
#define MENU_WIDGET_HOVERED_OVER_ICON_COLOR MENU_WIDGET_NOT_HOVERED_OVER_ICON_COLOR
#define MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR 0x76767636
#define MENU_WIDGET_CURRENT_MENU_BACKGROUND 0x13131336
#define MENU_WIDGET_HOVER_COLOR_FADE_SPEED 0.3f

typedef void (*menu_click_handler_t)(
    event_submissions_t *events);

// In this game, all menus have a sort of similar layout, so just group in this object
// There will always however be a maximum of 4 buttons
struct menu_layout_t {
    ui_box_t main_box;

    ui_box_t current_menu;
    smooth_linear_interpolation_t menu_slider;
    bool menu_in_out_transition;
    float menu_slider_x_max_size, menu_slider_y_max_size;

    uint32_t widget_count;
    menu_widget_t widgets[MAX_MENU_WIDGETS];

    int32_t current_button;
    int32_t current_open_menu;

    // If the proc == NULL, then event defaults to opening the menu slider
    menu_click_handler_t procs[MAX_MENU_WIDGETS];

    void init(
        menu_click_handler_t *handlers,
        const char **widget_image_paths,
        VkDescriptorSet *descriptor_sets,
        uint32_t widget_count);

    void submit();

    bool input(
        event_submissions_t *events,
        raw_input_t *input);

    bool menu_opened();

    void open_menu(
        uint32_t button);
};

#if 0                           // Need to do this later
struct widget_list_item_t {
    ui_box_t box;
    ui_text_t text;

    widget_color_t button_color;
};

struct widget_list_t {
    ui_box_t primary_button;
    ui_text_t primary_text;

    ui_box_t secondary_button;
    ui_text_t secondary_text;

    ui_box_t list_box;

    uint32_t item_count;
    widget_list_item_t *items;

    // 0xFFFF is unselected
    uint32_t selected_button;

    bool typing;
    ui_box_t typing_box;
    ui_input_text_t typing_text;
    widget_color_t typing_box_color;

    DECLARE_VOID_RENDERER_PROC(void, begin_filling_list,
        uint32_t item_count);

    
};
#endif
