#pragma once

#include <vk.hpp>
#include <math.hpp>

namespace vk {

struct lighting_ubo_data_t {
    vector4_t light_positions[MAX_LIGHTS];
    vector4_t ws_light_positions[MAX_LIGHTS];
    vector4_t light_directions[MAX_LIGHTS];
    vector4_t ws_light_directions[MAX_LIGHTS];
    vector4_t light_colors[MAX_LIGHTS];
    vector4_t vs_directional_light;
    vector4_t ws_directional_light;

    matrix4_t shadow_view_projection;
    matrix4_t shadow_view;
    matrix4_t shadow_projection;

    vector2_t light_screen_coord;

    int point_light_count;
};

struct shadow_box_t {
    matrix4_t view;
    matrix4_t projection;
    matrix4_t inverse_view;

    float near;
    float far;

    vector4_t ls_corners[8];
    
    union {
        struct {float x_min, x_max, y_min, y_max, z_min, z_max;};
        float corner_values[6];
    };

    void init(const vector3_t &light_dir, const vector3_t &up, float far);

    void update(
        const vector3_t &light_dir,
        float fov,
        float aspect,
        const vector3_t &ws_position,
        const vector3_t &ws_direction,
        const vector3_t &ws_up);
};

}
