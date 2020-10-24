#include "ui_hover.hpp"
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

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

bool ui_hover_over_box(
    ui_box_t *box,
    const vector2_t &cursor_position,
    vector2_t *relative_position) {
    vector2_t cursor = convert_pixel_to_ndc(cursor_position);

    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    float x_min = normalized_base_position.x,
        x_max = normalized_base_position.x + normalized_size.x,
        y_min = normalized_base_position.y,
        y_max = normalized_base_position.y + normalized_size.y;

    if (x_min < cursor.x && x_max > cursor.x
        && y_min < cursor.y && y_max > cursor.y) {
        if (relative_position)
            *relative_position = vector2_t(
                (cursor.x - x_min) / normalized_size.x,
                (cursor.y - y_min) / normalized_size.y);

        return true;
    }
    else {
        return false;
    }
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
