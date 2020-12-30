#include "ui_math.hpp"
#include <vulkan/vulkan.h>

#include <vk.hpp>

namespace ui {

::vector2_t convert_glsl_to_normalized(const ::vector2_t &position) {
    return position * 2.0f - 1.0f;
}

::vector2_t convert_normalized_to_glsl(const ::vector2_t &position) {
    return (position + 1.0f) / 2.0f;
}

ui::vector2_t glsl_to_pixel_coord(const ui::vector2_t &position, const VkExtent2D &resolution) {
    vector2_t ret(
        (int32_t)(position.fx * (float)resolution.width),
        (int32_t)(position.fy * (float)resolution.height));
    return ret;
}

::vector2_t glsl_to_pixel_coord(const ::vector2_t &position, const VkExtent2D &resolution) {
    ::vector2_t ret(
        (int32_t)(position.x * (float)resolution.width),
        (int32_t)(position.x * (float)resolution.height));
    return ret;
}

ui::vector2_t pixel_to_glsl_coord(const ui::vector2_t &position, const VkExtent2D &resolution) {
    vector2_t ret(
        (float)position.ix / (float)resolution.width,
        (float)position.iy / (float)resolution.height);
    return ret;
}

::vector2_t pixel_to_glsl_coord(const ::vector2_t &position, const VkExtent2D &resolution) {
    ::vector2_t ret(
        (float)position.x / (float)resolution.width,
        (float)position.x / (float)resolution.height);
    return ret;
}

uint32_t vec4_color_to_ui32b(const vector4_t &color) {
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

vector4_t ui32b_color_to_vec4(uint32_t color) {
    float r = (float)(color >> 24);
    float g = (float)((color >> 16) & 0xFF);
    float b = (float)((color >> 8) & 0xFF);
    float a = (float)((color >> 0) & 0xFF);

    return vector4_t(r, g, b, a) / 255.0f;
}

::vector2_t convert_pixel_to_ndc(
    const ::vector2_t &pixel_coord) {
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();

    // TODO: When supporting smaller res backbuffer, need to change this
    VkExtent2D swapchain_extent = {swapchain_info.width, swapchain_info.height};
    uint32_t rect2D_width, rect2D_height, rect2Dx, rect2Dy;

    rect2D_width = swapchain_extent.width;
    rect2D_height = swapchain_extent.height;
    rect2Dx = 0;
    rect2Dy = 0;

    ::vector2_t ndc;

    ndc.x = (float)(pixel_coord.x - rect2Dx) / (float)rect2D_width;
    ndc.y = 1.0f - (float)(pixel_coord.y - rect2Dy) / (float)rect2D_height;

    ndc.x = 2.0f * ndc.x - 1.0f;
    ndc.y = 2.0f * ndc.y - 1.0f;

    return ndc;
}


}
