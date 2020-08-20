#include "ui.hpp"
#include <cstddef>
#include "u_internal.hpp"
#include <common/meta.hpp>
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>
#include <common/allocators.hpp>

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static ui_stack_item_t stack_items[10];
static int32_t stack_item_count;

bool debugging_freeze = 0;

static void s_ui_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    // ...
    switch(event->type) {

    case ET_RECEIVED_AVAILABLE_SERVERS: {
        u_refresh_main_menu_server_page();
    } break;

    case ET_RESIZE_SURFACE: {
        // Need to reinitialise UI system
        u_main_menu_init();
        u_game_menu_init();
        u_sign_up_menu_init();
        u_hud_init();
    } break;

    case ET_META_REQUEST_ERROR: {
        event_meta_request_error_t *data = (event_meta_request_error_t *)event->data;

        u_handle_sign_up_failed(data->error_type);

        FL_FREE(data);
    } break;

    case ET_SIGN_UP_SUCCESS: {
        clear_ui_panels();
        push_ui_panel(USI_MAIN_MENU);

        // debugging_freeze = 1;
    } break;

    case ET_PRESSED_ESCAPE: {
        // debugging_freeze = 1;
        clear_ui_panels();
        push_ui_panel(USI_MAIN_MENU);
    } break;

    default: {
    } break;

    }
}

static listener_t ui_listener;

static struct font_t *global_font;
static texture_t ui_textures[UT_INVALID_TEXTURE];

struct font_t *u_game_font() {
    return global_font;
}

VkDescriptorSet u_texture(
    ui_texture_t type) {
    return ui_textures[type].descriptor;
}

static void s_ui_textures_init() {
    ui_textures[UT_PLAY_ICON] = create_texture("assets/textures/gui/play_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_BUILD_ICON] = create_texture("assets/textures/gui/build_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SETTINGS_ICON] = create_texture("assets/textures/gui/settings_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_QUIT_ICON] = create_texture("assets/textures/gui/quit_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SPAWN_ICON] = create_texture("assets/textures/gui/spawn_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_CROSSHAIRS] = create_texture("assets/textures/gui/crosshair.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
}

void ui_init(
    event_submissions_t *events) {
    stack_item_count = 0;

    ui_listener = set_listener_callback(
        &s_ui_event_listener,
        NULL,
        events);

    subscribe_to_event(ET_RECEIVED_AVAILABLE_SERVERS, ui_listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, ui_listener, events);
    subscribe_to_event(ET_RESIZE_SURFACE, ui_listener, events);
    subscribe_to_event(ET_META_REQUEST_ERROR, ui_listener, events);
    subscribe_to_event(ET_SIGN_UP_SUCCESS, ui_listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, ui_listener, events);

    global_font = load_font(
        "assets/font/fixedsys.fnt",
        "assets/font/fixedsys.png");

    // Initialise some textures
    s_ui_textures_init();
    u_main_menu_init();
    u_hud_init();
    u_game_menu_init();
    u_sign_up_menu_init();
}

void handle_ui_input(
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

        case USI_SIGN_UP: {
            u_sign_up_menu_input(events, raw_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }
}

void tick_ui(
    event_submissions_t *events) {
    if (stack_item_count) {
        switch (stack_items[stack_item_count - 1]) {
        case USI_MAIN_MENU: {
            u_submit_main_menu();
        } break;

        case USI_HUD: {
            u_submit_hud();
        } break;

        case USI_GAME_MENU: {
            u_submit_game_menu();
        } break;

        case USI_SIGN_UP: {
            if (!debugging_freeze)
                u_submit_sign_up_menu();
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

void push_ui_panel(ui_stack_item_t item) {
    stack_items[stack_item_count++] = item;
}

void pop_ui_panel() {
    if (stack_item_count > 0) {
        stack_item_count--;
    }
}

void clear_ui_panels() {
    stack_item_count = 0;
}
