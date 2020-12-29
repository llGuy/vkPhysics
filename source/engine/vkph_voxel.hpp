#pragma once

#include <stdint.h>
#include <math.hpp>

namespace vkph {

/*
 This is an 8-bit color value ( R R R G G G B B ) - this is what each bit represents
 starting from most significant to least significant.
*/
typedef uint8_t voxel_color_t;

struct voxel_t {
    voxel_color_t color;
    uint8_t value;
};

vector3_t b8_color_to_v3(voxel_color_t color);
voxel_color_t v3_color_to_b8(const vector3_t &color);
voxel_color_t b8v_color_to_b8(uint8_t r, uint8_t g, uint8_t b);

}
