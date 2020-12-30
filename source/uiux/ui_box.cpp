#include "ui_box.hpp"
#include <vk.hpp>

namespace ui {

static constexpr ::vector2_t RELATIVE_TO_ADD_VALUES[] {
    ::vector2_t(0.0f, 0.0f),
    ::vector2_t(0.0f, 1.0f),
    ::vector2_t(0.5f, 0.5f),
    ::vector2_t(1.0f, 0.0f),
    ::vector2_t(1.0f, 1.0f),
    ::vector2_t(0.0f, 0.0f),
};

static constexpr ::vector2_t RELATIVE_TO_FACTORS[] {
    ::vector2_t(0.0f, 0.0f),
    ::vector2_t(0.0f, -1.0f),
    ::vector2_t(-0.5f, -0.5f),
    ::vector2_t(-1.0f, 0.0f),
    ::vector2_t(-1.0f, -1.0f),
    ::vector2_t(0.0f, 0.0f),
};

void box_t::update_size(
    const VkExtent2D &backbuffer_resolution) {
    ui::vector2_t px_max_values;
    if (parent) {
        // relative_t to the parent aspect ratio
        px_max_values = glsl_to_pixel_coord(
            gls_max_values,
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});
    }
    else {
        px_max_values = glsl_to_pixel_coord(gls_max_values, backbuffer_resolution);
    }

    ui::vector2_t px_max_xvalue_coord(px_max_values.ix, (int32_t)((float)px_max_values.ix / aspect_ratio));
    // Check if, using glsl max x value, the y still fits in the given glsl max y value
    if (px_max_xvalue_coord.iy <= px_max_values.iy) {
        // Then use this new size;
        px_current_size = px_max_xvalue_coord;
    }
    else {
        // Then use y max value, and modify the x dependending on the new y
        ui::vector2_t px_max_yvalue_coord((uint32_t)((float)px_max_values.iy * aspect_ratio), px_max_values.iy);
        px_current_size = px_max_yvalue_coord;
    }
    if (parent) {
        gls_relative_size = ui::vector2_t(
            (float)px_current_size.ix / (float)parent->px_current_size.ix,
            (float)px_current_size.iy / (float)parent->px_current_size.iy);
        gls_current_size = ui::vector2_t(
            (float)px_current_size.ix / (float)backbuffer_resolution.width,
            (float)px_current_size.iy / (float)backbuffer_resolution.height);
    }
    else {
        gls_current_size = pixel_to_glsl_coord(px_current_size, backbuffer_resolution);
        gls_relative_size = pixel_to_glsl_coord(px_current_size, backbuffer_resolution);
    }
}

void box_t::update_position(
    const VkExtent2D &backbuffer_resolution) {
    ::vector2_t gls_size = gls_relative_size.to_fvec2();
    ::vector2_t gls_relative_position;
    if (relative_position.type == GLSL) {
        gls_relative_position = relative_position.to_fvec2();
    }
    if (relative_position.type == PIXEL) {
        gls_relative_position = pixel_to_glsl_coord(
            relative_position,
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy}).to_fvec2();
    }

    gls_relative_position += RELATIVE_TO_ADD_VALUES[relative_to];
    gls_relative_position += RELATIVE_TO_FACTORS[relative_to] * gls_size;

    if (parent) {
        ui::vector2_t px_size = glsl_to_pixel_coord(
            ui::vector2_t(gls_size.x, gls_size.y),
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});
        
        ui::vector2_t px_relative_position = glsl_to_pixel_coord(
            ui::vector2_t(gls_relative_position.x, gls_relative_position.y),
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});

        ivector2_t px_real_position = parent->px_position.to_ivec2() + px_relative_position.to_ivec2();


        gls_relative_position = pixel_to_glsl_coord(
            ui::vector2_t(px_real_position.x, px_real_position.y),
            backbuffer_resolution).to_fvec2();
    }

    gls_position = ui::vector2_t(gls_relative_position.x, gls_relative_position.y);

    if (relative_to == RT_CENTER) { 
        ::vector2_t normalized_size = gls_current_size.to_fvec2() * 2.0f;
        ::vector2_t normalized_base_position = ::vector2_t(0.0f) - normalized_size / 2.0f;
        normalized_base_position = (normalized_base_position + ::vector2_t(1.0f)) / 2.0f;

        gls_position.fx = normalized_base_position.x;
        gls_position.fy = normalized_base_position.y;
    }

    px_position = glsl_to_pixel_coord(
        gls_position,
        backbuffer_resolution);
}

void box_t::init(
    relative_to_t in_relative_to,
    float in_aspect_ratio,
    ui::vector2_t position /* coord_t space agnostic */,
    ui::vector2_t in_gls_max_values /* max_t X and Y size */,
    box_t *in_parent,
    uint32_t in_color,
    VkExtent2D backbuffer_resolution) {
    parent = NULL;

    VkExtent2D dst_resolution;
    if (backbuffer_resolution.width == BOX_SPECIAL_RESOLUTION &&
        backbuffer_resolution.height == BOX_SPECIAL_RESOLUTION) {
        vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();
        dst_resolution = {swapchain_info.width, swapchain_info.height};
    }
    else {
        dst_resolution = backbuffer_resolution;
    }
    
    if (parent) {
        dst_resolution = VkExtent2D{ (uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy };
    }
    
    relative_position = position;
    parent = in_parent;
    aspect_ratio = in_aspect_ratio;
    gls_max_values = in_gls_max_values;
    
    update_size(dst_resolution);
    
    relative_to = in_relative_to;
    
    update_position(dst_resolution);
    
    this->color = in_color;
}

}
