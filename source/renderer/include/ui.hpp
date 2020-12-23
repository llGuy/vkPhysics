#pragma once

#include <vk.hpp>
#include <app.hpp>
#include <vulkan/vulkan.h>
#include <common/math.hpp>

namespace ui {

/*
  Basically initialises the only global state of UI that is needed.
 */
void init_submission();

/*
  Make sure to call this before passing the ui secondary command buffer
  to the main renderer.
 */
void render_submitted_ui(VkCommandBuffer transfer_cmdbuf, VkCommandBuffer ui_cmdbuf);

/*
  Every UI object is defined relative to a certain point. This allows for
  the objects to be better anchored when changing resolutions.
 */
enum relative_to_t {
    RT_LEFT_DOWN,
    RT_LEFT_UP,
    RT_RELATIVE_CENTER,
    RT_RIGHT_DOWN,
    RT_RIGHT_UP,
    RT_CENTER,
};

enum coordinate_type_t { PIXEL, GLSL };

/*
  In UI code, this type of vector is used. It allows for easy switching between
  pixel coordinates and normalised device coordinates.
 */
struct vector2_t {
    union {
        struct {int32_t ix, iy;};
        struct {float fx, fy;};
    };
    coordinate_type_t type;

    vector2_t(void) = default;
    vector2_t(float x, float y) : fx(x), fy(y), type(GLSL) {}
    vector2_t(int32_t x, int32_t y) : ix(x), iy(y), type(PIXEL) {}
    vector2_t(const ::ivector2_t &iv) : ix(iv.x), iy(iv.y) {}
    
    inline ::vector2_t to_fvec2(void) const { return ::vector2_t(fx, fy); }
    inline ::ivector2_t to_ivec2(void) const { return ::ivector2_t(ix, iy); }
};

constexpr uint32_t BOX_SPECIAL_RESOLUTION = 0xFFFFFFFF;

struct box_t {
    box_t *parent = nullptr;
    relative_to_t relative_to;
    vector2_t relative_position;
    vector2_t gls_position;
    vector2_t px_position;
    vector2_t gls_max_values;
    vector2_t px_current_size;
    vector2_t gls_current_size;
    vector2_t gls_relative_size;
    float aspect_ratio;
    uint32_t color;

    void init(
        relative_to_t in_relative_to,
        float in_aspect_ratio,
        vector2_t position /* coord_t space agnostic */,
        vector2_t in_gls_max_values /* max_t X and Y size */,
        box_t *in_parent,
        uint32_t in_color,
        VkExtent2D backbuffer_resolution = VkExtent2D{ BOX_SPECIAL_RESOLUTION, BOX_SPECIAL_RESOLUTION });
    
    void update_size(const VkExtent2D &backbuffer_resolution);
    void update_position(const VkExtent2D &backbuffer_resolution);
    void rotate_clockwise(float rad_angle);
};

/*
  Some helpful math.
 */
::vector2_t convert_glsl_to_normalized(const ::vector2_t &position);
::vector2_t convert_normalized_to_glsl(const ::vector2_t &position);
ui::vector2_t glsl_to_pixel_coord(const ui::vector2_t &position, const VkExtent2D &resolution);
::vector2_t glsl_to_pixel_coord(const ::vector2_t &position, const VkExtent2D &resolution);
ui::vector2_t pixel_to_glsl_coord(const ui::vector2_t &position, const VkExtent2D &resolution);
::vector2_t pixel_to_glsl_coord(const ::vector2_t &position, const VkExtent2D &resolution);
uint32_t vec4_color_to_ui32b(const vector4_t &color);
vector4_t ui32b_color_to_vec4(uint32_t color);
::vector2_t convert_pixel_to_ndc(const ::vector2_t &pixel_coord);

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

/*
  For submission.
 */
void mark_ui_textured_section(VkDescriptorSet set);
void push_textured_box(const struct box_t *box, ::vector2_t *uvs);
void push_textured_box(const struct box_t *box);
void push_color_box(const box_t *box);
void push_reversed_color_box(const box_t *box, const ::vector2_t &size);
// If secret == 1, this is a password (only '*' will show up)
void push_text(text_t *text, bool secret = 0);
void push_ui_input_text(bool render_cursor, bool secret, uint32_t cursor_color, input_text_t *input);

}
