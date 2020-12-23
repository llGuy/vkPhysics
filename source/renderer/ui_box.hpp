#pragma once

#include "ui_vector.hpp"

#include <vulkan/vulkan.h>

namespace ui {

enum relative_to_t {
    RT_LEFT_DOWN,
    RT_LEFT_UP,
    RT_RELATIVE_CENTER,
    RT_RIGHT_DOWN,
    RT_RIGHT_UP,
    RT_CENTER,
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
    
    void update_size(
        const VkExtent2D &backbuffer_resolution);

    void update_position(
        const VkExtent2D &backbuffer_resolution);

    void rotate_clockwise(
        float rad_angle);
};

}
