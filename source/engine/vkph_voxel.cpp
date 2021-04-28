#include "vkph_voxel.hpp"

namespace vkph {

enum { B8_R_MAX = 0b111, B8_G_MAX = 0b111, B8_B_MAX = 0b11 };

vector3_t b8_color_to_v3(voxel_color_t color) {
    uint8_t r_b8 = color >> 5;
    uint8_t g_b8 = (color >> 2) & 0b111;
    uint8_t b_b8 = (color) & 0b11;

    float r_f32 = (float)(r_b8) / (float)(B8_R_MAX);
    float g_f32 = (float)(g_b8) / (float)(B8_G_MAX);
    float b_f32 = (float)(b_b8) / (float)(B8_B_MAX);

    return vector3_t(r_f32, g_f32, b_f32);
}

voxel_color_t v3_color_to_b8(const vector3_t &color) {
    float r = color.r * (float)(B8_R_MAX);
    float g = color.g * (float)(B8_G_MAX);
    float b = color.b * (float)(B8_B_MAX);

    return ((uint8_t)r << 5) + ((uint8_t)g << 2) + ((uint8_t)b);
}

voxel_color_t b8v_color_to_b8(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 5) + (g << 2) + b;
}

}
