#pragma once

#include "ui_vector.hpp"

#include <common/math.hpp>
#include <vulkan/vulkan.hpp>

namespace ui {

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
