#include "renderer.hpp"
#include "r_internal.hpp"
#include <common/allocators.hpp>

vector2_t convert_glsl_to_normalized(
    const vector2_t &position) {
    return(position * 2.0f - 1.0f);
}

vector2_t convert_normalized_to_glsl(
    const vector2_t &position) {
    return((position + 1.0f) / 2.0f);
}

ui_vector2_t glsl_to_pixel_coord(
    const ui_vector2_t &position,
    const VkExtent2D &resolution) {
    ui_vector2_t ret(
        (int32_t)(position.fx * (float)resolution.width),
        (int32_t)(position.fy * (float)resolution.height));
    return(ret);
}

vector2_t glsl_to_pixel_coord(
    const vector2_t &position,
    const VkExtent2D &resolution) {
    vector2_t ret(
        (int32_t)(position.x * (float)resolution.width),
        (int32_t)(position.x * (float)resolution.height));
    return(ret);
}

ui_vector2_t pixel_to_glsl_coord(
    const ui_vector2_t &position,
    const VkExtent2D &resolution) {
    ui_vector2_t ret(
        (float)position.ix / (float)resolution.width,
        (float)position.iy / (float)resolution.height);
    return(ret);
}

vector2_t pixel_to_glsl_coord(
    const vector2_t &position,
    const VkExtent2D &resolution) {
    vector2_t ret(
        (float)position.x / (float)resolution.width,
        (float)position.x / (float)resolution.height);
    return(ret);
}

uint32_t vec4_color_to_ui32b(
    const vector4_t &color) {
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

vector4_t ui32b_color_to_vec4(
    uint32_t color) {
    float r = (float)(color >> 24);
    float g = (float)((color >> 16) & 0xFF);
    float b = (float)((color >> 8) & 0xFF);
    float a = (float)((color >> 0) & 0xFF);

    return(vector4_t(r, g, b, a) / 255.0f);
}

static constexpr vector2_t RELATIVE_TO_ADD_VALUES[] { vector2_t(0.0f, 0.0f),
    vector2_t(0.0f, 1.0f),
    vector2_t(0.5f, 0.5f),
    vector2_t(1.0f, 0.0f),
    vector2_t(1.0f, 1.0f)
};

static constexpr vector2_t RELATIVE_TO_FACTORS[] { vector2_t(0.0f, 0.0f),
    vector2_t(0.0f, -1.0f),
    vector2_t(-0.5f, -0.5f),
    vector2_t(-1.0f, 0.0f),
    vector2_t(-1.0f, -1.0f)
};

void ui_box_t::update_size(
    const VkExtent2D &backbuffer_resolution) {
    ui_vector2_t px_max_values;
    if (parent) {
        // relative_t to the parent aspect ratio
        px_max_values = glsl_to_pixel_coord(
            gls_max_values,
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});
    }
    else {
        px_max_values = glsl_to_pixel_coord(gls_max_values, backbuffer_resolution);
    }

    ui_vector2_t px_max_xvalue_coord(px_max_values.ix, (int32_t)((float)px_max_values.ix / aspect_ratio));
    // Check if, using glsl max x value, the y still fits in the given glsl max y value
    if (px_max_xvalue_coord.iy <= px_max_values.iy) {
        // Then use this new size;
        px_current_size = px_max_xvalue_coord;
    }
    else {
        // Then use y max value, and modify the x dependending on the new y
        ui_vector2_t px_max_yvalue_coord((uint32_t)((float)px_max_values.iy * aspect_ratio), px_max_values.iy);
        px_current_size = px_max_yvalue_coord;
    }
    if (parent) {
        gls_relative_size = ui_vector2_t(
            (float)px_current_size.ix / (float)parent->px_current_size.ix,
            (float)px_current_size.iy / (float)parent->px_current_size.iy);
        gls_current_size = ui_vector2_t(
            (float)px_current_size.ix / (float)backbuffer_resolution.width,
            (float)px_current_size.iy / (float)backbuffer_resolution.height);
    }
    else {
        gls_current_size = pixel_to_glsl_coord(px_current_size, backbuffer_resolution);
        gls_relative_size = pixel_to_glsl_coord(px_current_size, backbuffer_resolution);
    }
}

void ui_box_t::update_position(
    const VkExtent2D &backbuffer_resolution) {
    vector2_t gls_size = gls_relative_size.to_fvec2();
    vector2_t gls_relative_position;
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
        ui_vector2_t px_size = glsl_to_pixel_coord(
            ui_vector2_t(gls_size.x, gls_size.y),
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});
        
        ui_vector2_t px_relative_position = glsl_to_pixel_coord(
            ui_vector2_t(gls_relative_position.x, gls_relative_position.y),
            VkExtent2D{(uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy});

        ivector2_t px_real_position = parent->px_position.to_ivec2() + px_relative_position.to_ivec2();


        gls_relative_position = pixel_to_glsl_coord(
            ui_vector2_t(px_real_position.x, px_real_position.y),
            backbuffer_resolution).to_fvec2();
    }

    gls_position = ui_vector2_t(gls_relative_position.x, gls_relative_position.y);

    px_position = glsl_to_pixel_coord(
        gls_position,
        backbuffer_resolution);
}


void ui_box_t::init(
    relative_to_t in_relative_to, float in_aspect_ratio,
    ui_vector2_t position /* coord_t space agnostic */,
    ui_vector2_t in_gls_max_values /* max_t X and Y size */,
    ui_box_t *in_parent,
    const uint32_t &in_color,
    VkExtent2D backbuffer_resolution) {
    VkExtent2D dst_resolution = backbuffer_resolution;
    
    if (parent) {
        dst_resolution = VkExtent2D{ (uint32_t)parent->px_current_size.ix, (uint32_t)parent->px_current_size.iy };
    }
    
    relative_position = position;
    parent = in_parent;
    aspect_ratio = in_aspect_ratio;
    gls_max_values = in_gls_max_values;
    
    update_size(backbuffer_resolution);
    
    relative_to = in_relative_to;
    
    update_position(backbuffer_resolution);
    
    this->color = in_color;
}

static shader_t textured_quads_shader;
static shader_t colored_quads_shader;

void ui_core_init() {
    shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = LN_MALLOC(VkVertexInputBindingDescription, 1);
    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(gui_colored_vertex_t);

    binding_info.attribute_count = 1;
    binding_info.attribute_descriptions = LN_MALLOC(VkVertexInputAttributeDescription, 2);
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    binding_info.attribute_descriptions[0].location = 1;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32_UINT;
    binding_info.attribute_descriptions[0].offset = sizeof(vector2_t);

    const char *color_shader_paths[] = {
        "shaders/SPV/uiquad.vert.spv",
        "shaders/SPV/uiquad.frag.spv"
    };

    create_2d_shader(
        &binding_info,
        0,
        NULL,
        0,
        color_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        r_stage_before_final_render(),
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

void gui_textured_vertex_render_list_t::mark_section(
    VkDescriptorSet set) {
    if (section_count) {
        // Get current section
        vertex_section_t *next = &sections[section_count];

        next->section_start = sections[section_count - 1].section_start + sections[section_count - 1].section_size;
        // Starts at 0. Every vertex that gets pushed adds to this variable
        next->section_size = 0;

        textures[section_count] = set;

        ++section_count;
    }
    else {
        sections[0].section_start = 0;
        sections[0].section_size = 0;

        textures[0] = set;
            
        ++section_count;
    }
}

void gui_textured_vertex_render_list_t::push_vertex(
    const gui_textured_vertex_t &vertex) {
    vertices[vertex_count++] = vertex;
    sections[section_count - 1].section_size += 1;
}

void gui_textured_vertex_render_list_t::push_ui_box(
    const ui_box_t *box) {
    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    
}

void gui_textured_vertex_render_list_t::clear_containers() {
    section_count = 0;
    vertex_count = 0;
}

void gui_textured_vertex_render_list_t::sync_gpu_with_vertex_list(
    VkCommandBuffer command_buffer) {
    if (vertex_count) {
        update_gpu_buffer(
            command_buffer,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0,
            sizeof(gui_textured_vertex_t) * vertex_count,
            vertices,
            &vtx_buffer);
    }
}

void gui_textured_vertex_render_list_t::render_textured_quads(
    VkCommandBuffer command_buffer) {
    if (vertex_count) {
        VkViewport viewport = {};
        viewport.width = (float)r_swapchain_extent().width;
        viewport.height = (float)r_swapchain_extent().height;
        viewport.maxDepth = 1;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D rect = {};
        rect.extent = r_swapchain_extent();
        vkCmdSetScissor(command_buffer, 0, 1, &rect);

        vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            shader.pipeline);

        // Loop through each section, bind texture, and render the quads from the section
        for (uint32_t current_section = 0; current_section < section_count; ++current_section) {
            vertex_section_t *section = &sections[current_section];
                
            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                shader.layout,
                0,
                1,
                &textures[current_section],
                0,
                NULL);

            VkDeviceSize zero = 0;
            vkCmdBindVertexBuffers(
                command_buffer,
                0,
                1,
                &vtx_buffer.buffer,
                &zero);

            vkCmdDraw(
                command_buffer,
                section->section_size,
                1,
                section->section_start,
                0);
        }
    }
}
