#pragma once

#include "vk_render_pipeline.hpp"

namespace vk {

// Most of this code will be for the renderer's atmosphere model

struct atmosphere_model_t {
    matrix4_t inverse_projection;
    float width;
    float height;
    float light_direction_x;
    float light_direction_y;
    float light_direction_z;
    float eye_height;
    float rayleigh;
    float mie;
    float intensity;
    float scatter_strength;
    float rayleigh_strength;
    float mie_strength;
    float rayleigh_collection;
    float mie_collection;
    float air_color_r;
    float air_color_g;
    float air_color_b;
};

void atmosphere_model_default_values(atmosphere_model_t *model, const vector3_t &directional_light);

}
