#include "ui.hpp"
#include <common/math.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

static enum button_t {
    B_BROWSE_SERVER,
    B_BUILD_MAP,
    B_SETTINGS,
    B_QUIT,
    B_INVALID_MENU_BUTTON
} current_button;

static const char *button_names[5] = {
    "BROWSE_SERVER",
    "BUILD_MAP",
    "SETTINGS",
    "QUIT",
    "INVALID"
};
#define NOT_HOVERED_OVER_ICON_COLOR 0xFFFFFF36
#define NOT_HOVERED_OVER_BACKGROUND_COLOR 0x16161636
// #define HOVERED_OVER_ICON_COLOR 0x46464636
#define HOVERED_OVER_ICON_COLOR NOT_HOVERED_OVER_ICON_COLOR
#define HOVERED_OVER_BACKGROUND_COLOR 0x76767636

static struct widget_t {
    texture_t texture;
    // Outline of where the texture will be (to add some padding)
    ui_box_t box;
    // Box where the image will be rendered to
    ui_box_t image_box;

    smooth_linear_interpolation_v3_t background_color;
    smooth_linear_interpolation_v3_t icon_color;

    bool hovered_on;
} widgets[B_INVALID_MENU_BUTTON];

// This won't be visible
static ui_box_t main_box;

static void s_widget_init(
    button_t button,
    const char *image_path,
    float current_button_y,
    float button_size) {
    widgets[button].box.init(
        RT_RIGHT_UP,
        1.0f,
        ui_vector2_t(0.0f, current_button_y),
        ui_vector2_t(button_size, button_size),
        &main_box,
        NOT_HOVERED_OVER_BACKGROUND_COLOR);

    widgets[button].image_box.init(
        RT_RELATIVE_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        &widgets[button].box,
        NOT_HOVERED_OVER_ICON_COLOR);
    
    widgets[button].texture = create_texture(
        image_path,
        VK_FORMAT_R8G8B8A8_UNORM,
        NULL,
        0,
        0,
        VK_FILTER_LINEAR);

    vector4_t current_background_color = ui32b_color_to_vec4(0x16161636);
    widgets[button].background_color.current = vector3_t(current_background_color);

    vector4_t current_icon_color = ui32b_color_to_vec4(0xFFFFFF36);
    widgets[button].icon_color.current = vector3_t(current_icon_color);
}

void u_main_menu_init() {
    current_button = B_INVALID_MENU_BUTTON;
    memset(widgets, sizeof(widgets), 0);

    main_box.init(
        RT_CENTER,
        2.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        NULL,
        0x46464646);

    float button_size = 0.25f;
    float current_button_y = 0.0f;

    s_widget_init(
        B_BROWSE_SERVER,
        "assets/textures/gui/play_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;
    
    s_widget_init(
        B_BUILD_MAP,
        "assets/textures/gui/build_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;

    s_widget_init(
        B_SETTINGS,
        "assets/textures/gui/settings_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;

    s_widget_init(
        B_QUIT,
        "assets/textures/gui/quit_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;
}

void u_submit_main_menu() {
    for (uint32_t i = 0; i < B_INVALID_MENU_BUTTON; ++i) {
        mark_ui_textured_section(widgets[i].texture.descriptor);
        push_textured_ui_box(&widgets[i].image_box);

        push_colored_ui_box(&widgets[i].box);
    }
}
    
static bool s_hover_over_box(
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

static bool s_hover_over_button(
    button_t button,
    float cursor_x,
    float cursor_y) {
    return s_hover_over_box(
        &widgets[button].box,
        cursor_x,
        cursor_y);
}

#define HOVER_COLOR_FADE_SPEED 0.3f

static void s_start_color_fade_interpolation(
    widget_t *widget,
    uint32_t final_background,
    uint32_t final_icon) {
    // Start interpolation
    vector4_t background_final = ui32b_color_to_vec4(final_background);
    widget->background_color.set(
        1,
        widget->background_color.current,
        background_final,
        HOVER_COLOR_FADE_SPEED);

    vector4_t icon_final = ui32b_color_to_vec4(final_icon);

    widget->icon_color.set(
        1,
        widget->icon_color.current,
        icon_final,
        HOVER_COLOR_FADE_SPEED);
}

void u_main_menu_input(
    game_input_t *input) {
    float cursor_x = input->mouse_x, cursor_y = input->mouse_y;

    // Check if user is hovering over any buttons
    bool hovered_over_button = 0;

    for (uint32_t i = 0; i < (uint32_t)B_INVALID_MENU_BUTTON; ++i) {
        uint32_t prev_background, prev_icon, next_background, next_icon;

        bool start_interpolation = 0;

        if (s_hover_over_button((button_t)i, cursor_x, cursor_y)) {
            prev_background = NOT_HOVERED_OVER_BACKGROUND_COLOR;
            prev_icon = NOT_HOVERED_OVER_ICON_COLOR;
            next_background = HOVERED_OVER_BACKGROUND_COLOR;
            next_icon = HOVERED_OVER_ICON_COLOR;

            if (!widgets[i].hovered_on) {
                start_interpolation = 1;
            }

            current_button = (button_t)i;
            hovered_over_button = 1;
            widgets[i].hovered_on = 1;
        }
        else {
            prev_background = HOVERED_OVER_BACKGROUND_COLOR;
            prev_icon = HOVERED_OVER_ICON_COLOR;
            next_background = NOT_HOVERED_OVER_BACKGROUND_COLOR;
            next_icon = NOT_HOVERED_OVER_ICON_COLOR;

            if (widgets[i].hovered_on) {
                start_interpolation = 1;
            }

            widgets[i].hovered_on = 0;
        }

        if (start_interpolation) {
            s_start_color_fade_interpolation(
                &widgets[i],
                next_background,
                next_icon);
        }

        if (
            widgets[i].background_color.in_animation ||
            widgets[i].icon_color.in_animation) {
            widgets[i].background_color.animate(surface_delta_time());
            widgets[i].icon_color.animate(surface_delta_time());

            vector4_t current_background = vector4_t(
                widgets[i].background_color.current.r,
                widgets[i].background_color.current.g,
                widgets[i].background_color.current.b,
                ((float)0x36) / 255.0f);

            widgets[i].box.color = vec4_color_to_ui32b(current_background);

            vector4_t current_icon = vector4_t(
                widgets[i].icon_color.current.r,
                widgets[i].icon_color.current.g,
                widgets[i].icon_color.current.b,
                ((float)0x36) / 255.0f);

            widgets[i].image_box.color = vec4_color_to_ui32b(current_icon);
        }
        else {
            widgets[i].image_box.color = next_icon;
            widgets[i].box.color = next_background;

            widgets[i].background_color.current = vector3_t(ui32b_color_to_vec4(next_background));
            widgets[i].icon_color.current = vector3_t(ui32b_color_to_vec4(next_icon));
        }
    }

    if (!hovered_over_button) {
        current_button = B_INVALID_MENU_BUTTON;
    }
}
