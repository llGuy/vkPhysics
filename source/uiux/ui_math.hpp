#pragma once

#include <stdint.h>
#include <math.hpp>
#include <vulkan/vulkan.h>

namespace ui {

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

}
