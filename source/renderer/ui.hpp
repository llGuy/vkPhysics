#pragma once

#include <vulkan/vulkan.h>
#include <common/tools.hpp>

// User interface stuff
enum coordinate_type_t { PIXEL, GLSL };

struct ui_vector2_t {
    union {
        struct {int32_t ix, iy;};
        struct {float fx, fy;};
    };
    coordinate_type_t type;

    ui_vector2_t(void) = default;
    ui_vector2_t(float x, float y) : fx(x), fy(y), type(GLSL) {}
    ui_vector2_t(int32_t x, int32_t y) : ix(x), iy(y), type(PIXEL) {}
    ui_vector2_t(const ivector2_t &iv) : ix(iv.x), iy(iv.y) {}
    
    inline vector2_t to_fvec2(void) const {
        return vector2_t(fx, fy);
    }

    inline ivector2_t to_ivec2(void) const {
        return ivector2_t(ix, iy);
    }
};

enum relative_to_t { LEFT_DOWN, LEFT_UP, CENTER, RIGHT_DOWN, RIGHT_UP };

// Math functions
inline vector2_t convert_glsl_to_normalized(const vector2_t &position) {
    return(position * 2.0f - 1.0f);
}

inline vector2_t convert_normalized_to_glsl(const vector2_t &position) {
    return((position + 1.0f) / 2.0f);
}

inline ui_vector2_t glsl_to_pixel_coord(const ui_vector2_t &position, const VkExtent2D &resolution) {
    ui_vector2_t ret((int32_t)(position.fx * (float)resolution.width), (int32_t)(position.fy * (float)resolution.height));
    return(ret);
}

inline vector2_t glsl_to_pixel_coord(const vector2_t &position, const VkExtent2D &resolution) {
    vector2_t ret((int32_t)(position.x * (float)resolution.width), (int32_t)(position.x * (float)resolution.height));
    return(ret);
}

inline ui_vector2_t pixel_to_glsl_coord(const ui_vector2_t &position, const VkExtent2D &resolution) {
    ui_vector2_t ret((float)position.ix / (float)resolution.width,
                     (float)position.iy / (float)resolution.height);
    return(ret);
}

inline vector2_t pixel_to_glsl_coord(const vector2_t &position, const VkExtent2D &resolution) {
    vector2_t ret((float)position.x / (float)resolution.width,
                  (float)position.x / (float)resolution.height);
    return(ret);
}

inline uint32_t vec4_color_to_ui32b(const vector4_t &color) {
    float xf = color.x * 255.0f;
    float yf = color.y * 255.0f;
    float zf = color.z * 255.0f;
    float wf = color.w * 255.0f;
    uint32_t xui = (uint32_t)xf;
    uint32_t yui = (uint32_t)yf;
    uint32_t zui = (uint32_t)zf;
    uint32_t wui = (uint32_t)wf;
    return (xui << 24) | (yui << 16) | (zui << 8) | wui;
}

inline vector4_t ui32b_color_to_vec4(uint32_t color) {
    float r = (float)(color >> 24);
    float g = (float)((color >> 16) & 0xFF);
    float b = (float)((color >> 8) & 0xFF);
    float a = (float)((color >> 0) & 0xFF);

    return(vector4_t(r, g, b, a) / 255.0f);
}

struct gui_colored_vertex_t {
    vector2_t position;
    uint32_t color;
};

struct gui_textured_vertex_t {
    vector2_t position;
    vector2_t uvs;
    uint32_t color;
};

struct ui_box_t {
    ui_box_t *parent {nullptr};
    relative_to_t relative_to;
    ui_vector2_t relative_position;
    ui_vector2_t gls_position;
    ui_vector2_t px_position;
    ui_vector2_t gls_max_values;
    ui_vector2_t px_current_size;
    ui_vector2_t gls_current_size;
    ui_vector2_t gls_relative_size;
    float aspect_ratio;
    uint32_t color;

    void initialize(
        relative_to_t relative_to,
        float aspect_ratio,
        ui_vector2_t position /* coord_t space agnostic */,
        ui_vector2_t gls_max_values /* max_t X and Y size */,
        ui_box_t *parent,
        const uint32_t &color,
        VkExtent2D backbuffer_resolution = {});
    
    void update_size(const VkExtent2D &backbuffer_resolution);
    void update_position(const VkExtent2D &backbuffer_resolution);

    void rotate_clockwise(float rad_angle);
};
