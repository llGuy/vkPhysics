#pragma once

#include "ui_box.hpp"
#include "ui_math.hpp"
#include <vk.hpp>
#include <app.hpp>

namespace ui {

struct font_character_t {
    char character_value;
    ::vector2_t uvs_base;
    ::vector2_t uvs_size;
    ::vector2_t display_size;
    ::vector2_t offset;
    float advance;
};

struct font_t {
    int32_t char_count;
    vk::texture_t font_img;

    font_character_t font_characters[0xFF];

    void load(const char *fnt_file, const char *png_file);
};

struct text_t {
    box_t *dst_box;
    struct font_t *font;
    
    // Max characters = 500
    uint32_t colors[500] = {};
    char characters[500] = {};
    uint32_t char_count = 0;

    enum font_stream_box_relative_to_t { TOP, BOTTOM, CENTER };

    font_stream_box_relative_to_t relative_to;

    // Relative to xadvance
    float x_start;
    float y_start;

    uint32_t chars_per_line;

    // Relative to xadvance
    float line_height;

    void init(
        box_t *box,
        font_t *font,
        font_stream_box_relative_to_t relative_to,
        float px_x_start,
        float px_y_start,
        uint32_t chars_per_line,
        float line_height);

    void draw_char(char character, uint32_t color);
    void draw_string(const char *string, uint32_t color);

    void null_terminate();
};

struct input_text_t {
    uint32_t cursor_position;
    text_t text;

    uint32_t cursor_fade;

    uint32_t text_color;
    bool fade_in_or_out = 0;

    void input(const app::raw_input_t *raw_input);
    const char *get_string();
};

}
