#pragma once

#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>

#include "ui_hover.hpp"

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