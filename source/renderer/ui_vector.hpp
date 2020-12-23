#pragma once

#include <stdint.h>

#include <common/math.hpp>

namespace ui {

enum coordinate_type_t { PIXEL, GLSL };

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


}
