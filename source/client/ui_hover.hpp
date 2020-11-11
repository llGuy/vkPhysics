#pragma once

#include <common/math.hpp>

struct color_pair_t {
    uint32_t current_foreground, current_background;
};

struct widget_color_t {
    uint32_t unhovered_background;
    uint32_t hovered_background;
    uint32_t unhovered_foreground;
    uint32_t hovered_foreground;

    bool is_hovered_on;

    linear_interpolation_v4_t background_color;
    linear_interpolation_v4_t foreground_color;

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

// Relative position will be from 0 -> 1
bool ui_hover_over_box(
    struct ui_box_t *box,
    const vector2_t &cursor_position,
    vector2_t *relative_position = NULL);
