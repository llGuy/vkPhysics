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

#define HOVERED_OVER_ICON_COLOR 0x46464636
#define HOVERED_OVER_BACKGROUND_COLOR 0x76767636
#define NOT_HOVERED_OVER_ICON_COLOR 0xFFFFFF36
#define NOT_HOVERED_OVER_BACKGROUND_COLOR 0x16161636

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

void u_main_menu_input(
    game_input_t *input) {
    float cursor_x = input->mouse_x, cursor_y = input->mouse_y;

    // Check if user is hovering over any buttons
    bool hovered_over_button = 0;

    for (uint32_t i = 0; i < (uint32_t)B_INVALID_MENU_BUTTON; ++i) {
        if (s_hover_over_button((button_t)i, cursor_x, cursor_y)) {
            hovered_over_button = 1;
            
            // Wasn't hovered over before
            if (current_button != i) {
                // Start interpolation
                vector4_t background_final = ui32b_color_to_vec4(HOVERED_OVER_BACKGROUND_COLOR);
                widgets[i].background_color.set(
                    1,
                    widgets[i].background_color.current,
                    background_final,
                    HOVER_COLOR_FADE_SPEED);

                vector4_t icon_final = ui32b_color_to_vec4(HOVERED_OVER_ICON_COLOR);

                widgets[i].icon_color.set(
                    1,
                    widgets[i].icon_color.current,
                    icon_final,
                    HOVER_COLOR_FADE_SPEED);
            }
            
            current_button = (button_t)i;
            widgets[i].hovered_on = 1;

            if (widgets[i].background_color.in_animation) {
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
            //main_menu.icon_color_interpolation.animate(raw_input->dt);
        }
        else {
            // Need to switch animation direction
            if (widgets[i].hovered_on) {
                current_button = B_INVALID_MENU_BUTTON;

                vector4_t background_final = ui32b_color_to_vec4(NOT_HOVERED_OVER_BACKGROUND_COLOR);
                widgets[i].background_color.set(
                    1,
                    widgets[i].background_color.current,
                    background_final,
                    HOVER_COLOR_FADE_SPEED);

                vector4_t icon_final = ui32b_color_to_vec4(NOT_HOVERED_OVER_ICON_COLOR);

                widgets[i].icon_color.set(
                    1,
                    widgets[i].icon_color.current,
                    icon_final,
                    HOVER_COLOR_FADE_SPEED);
            }
            else {
                if (widgets[i].background_color.in_animation) {
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
                    widgets[i].image_box.color = NOT_HOVERED_OVER_ICON_COLOR;
                    widgets[i].box.color = NOT_HOVERED_OVER_BACKGROUND_COLOR;

                    widgets[i].background_color.current = vector3_t(ui32b_color_to_vec4(NOT_HOVERED_OVER_BACKGROUND_COLOR));
                    widgets[i].icon_color.current = vector3_t(ui32b_color_to_vec4(NOT_HOVERED_OVER_ICON_COLOR));
                }
            }

            widgets[i].hovered_on = 0;
        }
    }
}
