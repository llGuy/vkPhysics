#include "ui.hpp"
#include "u_internal.hpp"
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

enum ui_stack_item_t {
    USI_MAIN_MENU,
    USI_HUD,
    USI_GAME_MENU,
    USI_INVALID,
};

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static ui_stack_item_t stack_items[10];
static int32_t stack_item_count;

static void s_ui_event_listener(
    void *object,
    event_t *event) {
    // ...
    switch(event->type) {

    case ET_LAUNCH_MAIN_MENU_SCREEN: {
        stack_items[0] = USI_MAIN_MENU;
        stack_item_count = 1;
    } break;

    case ET_LAUNCH_GAME_MENU_SCREEN: {
        stack_items[0] = USI_GAME_MENU;
        stack_item_count = 1;
    } break;

    case ET_CLEAR_MENUS: {
        stack_item_count = 0;

        u_clear_main_menu();
    } break;

    case ET_RESIZE_SURFACE: {
        // Need to reinitialise UI system
    } break;

    default: {
    } break;

    }
}

static listener_t ui_listener;

static struct font_t *global_font;

struct font_t *u_game_font() {
    return global_font;
}

void ui_init(
    event_submissions_t *events) {
    stack_item_count = 0;

    ui_listener = set_listener_callback(
        &s_ui_event_listener,
        NULL,
        events);

    subscribe_to_event(
        ET_LAUNCH_MAIN_MENU_SCREEN,
        ui_listener,
        events);
    subscribe_to_event(
        ET_CLEAR_MENUS,
        ui_listener,
        events);
    subscribe_to_event(
        ET_LAUNCH_GAME_MENU_SCREEN,
        ui_listener,
        events);

    global_font = load_font(
        "assets/font/fixedsys.fnt",
        "assets/font/fixedsys.png");

    u_main_menu_init();

    u_hud_init();

    u_game_menu_init();
}

void tick_ui(
    event_submissions_t *events) {
    raw_input_t *raw_input = get_raw_input();

    if (stack_item_count) {
        ui_stack_item_t last = stack_items[stack_item_count - 1];

        switch (last) {
        case USI_MAIN_MENU: {
            u_main_menu_input(events, raw_input);
        } break;

        case USI_GAME_MENU: {
            u_game_menu_input(events, raw_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }

    for (uint32_t i = 0; i < stack_item_count; ++i) {
        switch (stack_items[i]) {
        case USI_MAIN_MENU: {
            u_submit_main_menu();
        } break;

        case USI_HUD: {
            u_submit_hud();
        } break;

        case USI_GAME_MENU: {
            u_submit_game_menu();
        } break;

        default: {
        } break;
        }
    }
}

void widget_color_t::init(
    uint32_t iunhovered_background,
    uint32_t ihovered_background,
    uint32_t iunhovered_foreground,
    uint32_t ihovered_foreground) {
    unhovered_background = iunhovered_background;
    hovered_background = ihovered_background;
    unhovered_foreground = iunhovered_foreground;
    hovered_foreground = ihovered_foreground;

    vector4_t current_background = ui32b_color_to_vec4(unhovered_background);
    background_color.current = vector3_t(current_background);

    vector4_t current_foreground = ui32b_color_to_vec4(unhovered_foreground);
    foreground_color.current = vector3_t(current_foreground);

    is_hovered_on = 0;
}

void widget_color_t::start_interpolation(
    float interpolation_speed,
    uint32_t final_background,
    uint32_t final_foreground) {
    // Start interpolation
    vector4_t background_final = ui32b_color_to_vec4(final_background);
    background_color.set(
        1,
        background_color.current,
        background_final,
        interpolation_speed);

    vector4_t icon_final = ui32b_color_to_vec4(final_foreground);

    foreground_color.set(
        1,
        foreground_color.current,
        icon_final,
        interpolation_speed);
}

color_pair_t widget_color_t::update(
    float interpolation_speed,
    bool hovered_on) {
    color_pair_t result = {};

    uint32_t next_background, next_icon;

    bool should_start_interpolation = 0;

    if (hovered_on) {
        next_background = hovered_background;
        next_icon = hovered_foreground;

        if (!is_hovered_on) {
            should_start_interpolation = 1;
        }

        is_hovered_on = 1;
    }
    else {
        next_background = unhovered_background;
        next_icon = unhovered_foreground;

        if (is_hovered_on) {
            should_start_interpolation = 1;
        }

        is_hovered_on = 0;
    }

    if (should_start_interpolation) {
        start_interpolation(
            interpolation_speed,
            next_background,
            next_icon);
    }

    if (
        background_color.in_animation ||
        foreground_color.in_animation) {
        background_color.animate(surface_delta_time());
        foreground_color.animate(surface_delta_time());

        vector4_t current_background = vector4_t(
            background_color.current.r,
            background_color.current.g,
            background_color.current.b,
            ((float)0x36) / 255.0f);

        result.current_background = vec4_color_to_ui32b(current_background);

        vector4_t current_icon = vector4_t(
            foreground_color.current.r,
            foreground_color.current.g,
            foreground_color.current.b,
            ((float)0x36) / 255.0f);

        result.current_foreground = vec4_color_to_ui32b(current_icon);
    }
    else {
        result.current_foreground = next_icon;
        result.current_background = next_background;

        background_color.current = vector3_t(ui32b_color_to_vec4(next_background));
        foreground_color.current = vector3_t(ui32b_color_to_vec4(next_icon));
    }

    return result;
}

bool u_hover_over_box(
    ui_box_t *box,
    float cursor_x,
    float cursor_y) {
    vector2_t cursor = convert_pixel_to_ndc(vector2_t(cursor_x, cursor_y));

    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    float x_min = normalized_base_position.x,
        x_max = normalized_base_position.x + normalized_size.x,
        y_min = normalized_base_position.y,
        y_max = normalized_base_position.y + normalized_size.y;

    if (x_min < cursor.x && x_max > cursor.x
        && y_min < cursor.y && y_max > cursor.y) {
        return(true);
    }
    else {
        return(false);
    }
}
