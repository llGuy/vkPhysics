#include "renderer.hpp"
#include "r_internal.hpp"
#include "renderer/input.hpp"
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
        vector2_t(1.0f, 1.0f),
        vector2_t(0.0f, 0.0f),
        };

static constexpr vector2_t RELATIVE_TO_FACTORS[] { vector2_t(0.0f, 0.0f),
        vector2_t(0.0f, -1.0f),
        vector2_t(-0.5f, -0.5f),
        vector2_t(-1.0f, 0.0f),
        vector2_t(-1.0f, -1.0f),
        vector2_t(0.0f, 0.0f),
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

    if (relative_to == RT_CENTER) { 
        vector2_t normalized_size = gls_current_size.to_fvec2() * 2.0f;
        vector2_t normalized_base_position = vector2_t(0.0f) - normalized_size / 2.0f;
        normalized_base_position = (normalized_base_position + vector2_t(1.0f)) / 2.0f;

        gls_position.fx = normalized_base_position.x;
        gls_position.fy = normalized_base_position.y;
    }

    px_position = glsl_to_pixel_coord(
        gls_position,
        backbuffer_resolution);
}

void ui_box_t::init(
    relative_to_t in_relative_to,
    float in_aspect_ratio,
    ui_vector2_t position /* coord_t space agnostic */,
    ui_vector2_t in_gls_max_values /* max_t X and Y size */,
    ui_box_t *in_parent,
    uint32_t in_color,
    VkExtent2D backbuffer_resolution) {
    parent = NULL;

    VkExtent2D dst_resolution;
    if (backbuffer_resolution.width == UI_BOX_SPECIAL_RESOLUTION &&
        backbuffer_resolution.height == UI_BOX_SPECIAL_RESOLUTION) {
        dst_resolution = r_swapchain_extent();
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

struct vertex_section_t {
    uint32_t section_start, section_size;
};

static struct gui_textured_vertex_render_list_t {
    uint32_t section_count = 0;
    // Index where there is a transition to the next texture to bind for the following vertices
    vertex_section_t sections[MAX_TEXTURES_IN_RENDER_LIST] = {};
    // Corresponds to the same index as the index of the current section
    VkDescriptorSet textures[MAX_TEXTURES_IN_RENDER_LIST] = {};

    uint32_t vertex_count;
    gui_textured_vertex_t vertices[MAX_QUADS_TO_RENDER * 6];

    gpu_buffer_t vtx_buffer;
} textured_list;

static struct gui_colored_vertex_render_list_t {
    uint32_t vertex_count;
    gui_colored_vertex_t vertices[MAX_QUADS_TO_RENDER * 6];

    gpu_buffer_t vtx_buffer;
} colored_list;

static shader_t textured_quads_shader;
static shader_t colored_quads_shader;

static void s_colored_shader_init() {
    shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = LN_MALLOC(VkVertexInputBindingDescription, 1);
    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(gui_colored_vertex_t);

    binding_info.attribute_count = 2;
    binding_info.attribute_descriptions = LN_MALLOC(VkVertexInputAttributeDescription, 2);
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    binding_info.attribute_descriptions[1].location = 1;
    binding_info.attribute_descriptions[1].binding = 0;
    binding_info.attribute_descriptions[1].format = VK_FORMAT_R32_UINT;
    binding_info.attribute_descriptions[1].offset = sizeof(vector2_t);

    const char *color_shader_paths[] = {
        "shaders/SPV/uiquad.vert.spv",
        "shaders/SPV/uiquad.frag.spv"
    };

    colored_quads_shader = create_2d_shader(
        &binding_info,
        0,
        NULL,
        0,
        color_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        r_ui_stage(),
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        AB_ONE_MINUS_SRC_ALPHA);
}

static void s_textured_shader_init() {
    shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = LN_MALLOC(VkVertexInputBindingDescription, 1);
    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(gui_textured_vertex_t);

    binding_info.attribute_count = 3;
    binding_info.attribute_descriptions = LN_MALLOC(VkVertexInputAttributeDescription, 3);
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    binding_info.attribute_descriptions[1].location = 1;
    binding_info.attribute_descriptions[1].binding = 0;
    binding_info.attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[1].offset = sizeof(vector2_t);

    binding_info.attribute_descriptions[2].location = 2;
    binding_info.attribute_descriptions[2].binding = 0;
    binding_info.attribute_descriptions[2].format = VK_FORMAT_R32_UINT;
    binding_info.attribute_descriptions[2].offset = sizeof(vector2_t) + sizeof(vector2_t);

    const char *texture_shader_paths[] = {
        "shaders/SPV/uiquadtex.vert.spv",
        "shaders/SPV/uiquadtex.frag.spv"
    };

    VkDescriptorType texture_descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    textured_quads_shader = create_2d_shader(
        &binding_info,
        0,
        &texture_descriptor_type,
        1,
        texture_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        r_ui_stage(),
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        AB_ONE_MINUS_SRC_ALPHA);
}

void ui_rendering_init() {
    s_colored_shader_init();
    s_textured_shader_init();

    colored_list.vtx_buffer = create_gpu_buffer(
        MAX_QUADS_TO_RENDER * 6 * sizeof(gui_colored_vertex_t),
        colored_list.vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    textured_list.vtx_buffer = create_gpu_buffer(
        MAX_QUADS_TO_RENDER * 6 * sizeof(gui_textured_vertex_t),
        textured_list.vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void mark_ui_textured_section(
    VkDescriptorSet set) {
    if (textured_list.section_count) {
        // Get current section
        vertex_section_t *next = &textured_list.sections[textured_list.section_count];

        next->section_start = textured_list.sections[textured_list.section_count - 1].section_start + textured_list.sections[textured_list.section_count - 1].section_size;
        // Starts at 0. Every vertex that gets pushed adds to this variable
        next->section_size = 0;

        textured_list.textures[textured_list.section_count] = set;

        ++textured_list.section_count;
    }
    else {
        textured_list.sections[0].section_start = 0;
        textured_list.sections[0].section_size = 0;

        textured_list.textures[0] = set;
            
        ++textured_list.section_count;
    }
}

void push_textured_vertex(
    const gui_textured_vertex_t &vertex) {
    textured_list.vertices[textured_list.vertex_count++] = vertex;
    textured_list.sections[textured_list.section_count - 1].section_size += 1;
}

void push_textured_ui_box(
    const ui_box_t *box,
    vector2_t *uvs) {
    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;
    
    push_textured_vertex({normalized_base_position, uvs[0], box->color});
    push_textured_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), uvs[1], box->color});
    push_textured_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), uvs[2], box->color});
    push_textured_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), uvs[3], box->color});
    push_textured_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), uvs[4], box->color});
    push_textured_vertex({normalized_base_position + normalized_size, uvs[5], box->color});
}

void push_textured_ui_box(
    const ui_box_t *box) {
    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;
    
    push_textured_vertex({normalized_base_position, vector2_t(0.0f, 1.0f), box->color});
    push_textured_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), vector2_t(0.0f), box->color});
    push_textured_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), vector2_t(1.0f), box->color});
    push_textured_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), vector2_t(0.0f), box->color});
    push_textured_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), vector2_t(1.0f), box->color});
    push_textured_vertex({normalized_base_position + normalized_size, vector2_t(1.0f, 0.0f), box->color});
}

void clear_texture_list_containers() {
    textured_list.section_count = 0;
    textured_list.vertex_count = 0;
}

void sync_gpu_with_textured_vertex_list(
    VkCommandBuffer command_buffer) {
    if (textured_list.vertex_count) {
        update_gpu_buffer(
            command_buffer,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0,
            sizeof(gui_textured_vertex_t) * textured_list.vertex_count,
            textured_list.vertices,
            &textured_list.vtx_buffer);

        if (sizeof(gui_textured_vertex_t) * textured_list.vertex_count > 65536) {
            LOG_ERROR("WE HAVE A BIG PROBLEM\n");
        }
    }
}

void render_textured_quads(
    VkCommandBuffer command_buffer) {
    if (textured_list.vertex_count) {
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
            textured_quads_shader.pipeline);

        // Loop through each section, bind texture, and render the quads from the section
        for (uint32_t current_section = 0; current_section < textured_list.section_count; ++current_section) {
            vertex_section_t *section = &textured_list.sections[current_section];
                
            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                textured_quads_shader.layout,
                0,
                1,
                &textured_list.textures[current_section],
                0,
                NULL);

            VkDeviceSize zero = 0;
            vkCmdBindVertexBuffers(
                command_buffer,
                0,
                1,
                &textured_list.vtx_buffer.buffer,
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

void push_colored_vertex(
    const gui_colored_vertex_t &vertex) {
    colored_list.vertices[colored_list.vertex_count++] = vertex;
}

void push_colored_ui_box(
    const ui_box_t *box) {
    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    push_colored_vertex({normalized_base_position, box->color});
    push_colored_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), box->color});
    push_colored_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), box->color});
    push_colored_vertex({normalized_base_position + vector2_t(0.0f, normalized_size.y), box->color});
    push_colored_vertex({normalized_base_position + vector2_t(normalized_size.x, 0.0f), box->color});
    push_colored_vertex({normalized_base_position + normalized_size, box->color});
}

void push_reversed_colored_ui_box(
    const ui_box_t *box,
    const vector2_t &size) {
    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    normalized_base_position += size;

    push_colored_vertex({normalized_base_position, box->color});
    push_colored_vertex({normalized_base_position - vector2_t(0.0f, normalized_size.y), box->color});
    push_colored_vertex({normalized_base_position - vector2_t(normalized_size.x, 0.0f), box->color});
    push_colored_vertex({normalized_base_position - vector2_t(0.0f, normalized_size.y), box->color});
    push_colored_vertex({normalized_base_position - vector2_t(normalized_size.x, 0.0f), box->color});
    push_colored_vertex({normalized_base_position - normalized_size, box->color});
}

static ivector2_t get_px_cursor_position(
    ui_box_t *box,
    ui_text_t *text,
    const VkExtent2D &resolution) {
    uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
    uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
    px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
    uint32_t px_char_height = (uint32_t)(text->line_height * (float)px_char_width);
    
    ivector2_t px_cursor_position;
    switch(text->relative_to) {
    case ui_text_t::font_stream_box_relative_to_t::TOP: {
            uint32_t px_box_top = box->px_position.iy + box->px_current_size.iy;
            px_cursor_position = ivector2_t(x_start + box->px_position.ix, px_box_top - (uint32_t)(text->y_start * (float)px_char_width));
            break;
        }
    case ui_text_t::font_stream_box_relative_to_t::BOTTOM: {
            uint32_t px_box_bottom = box->px_position.iy;
            px_cursor_position = ivector2_t(x_start + box->px_position.ix, px_box_bottom + ((uint32_t)(text->y_start * (float)(px_char_width)) + px_char_height));
            break;
        }
    }
    return(px_cursor_position);
}

void push_ui_text(
    ui_text_t *text,
    bool secret) {
    VkExtent2D resolution = r_swapchain_extent();

    ui_box_t *box = text->dst_box;

    uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
    uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
    px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
    uint32_t px_char_height = (uint32_t)(text->line_height * (float)px_char_width);
    
    ivector2_t px_cursor_position = get_px_cursor_position(box, text, resolution);

    uint32_t chars_since_new_line = 0;

    char *characters = text->characters;

    if (secret) {
        characters = LN_MALLOC(char, text->char_count);
        for (uint32_t i = 0; i < text->char_count; ++i) {
            characters[i] = '*';
        }
    }
    
    for (uint32_t character = 0;
         character < text->char_count;) {
        char current_char_value = characters[character];
        if (current_char_value == '\n') {
            px_cursor_position.y -= px_char_height;
            px_cursor_position.x = x_start + box->px_position.ix;
            ++character;
            chars_since_new_line = 0;
            continue;
        }
        
        font_character_t *font_character_data = &text->font->font_characters[(uint32_t)current_char_value];
        uint32_t color = text->colors[character];

        // Top left
        
        vector2_t px_character_size = vector2_t(vector2_t(font_character_data->display_size) * (float)px_char_width);
        vector2_t px_character_base_position =  vector2_t(px_cursor_position) + vector2_t(vector2_t(font_character_data->offset) * (float)px_char_width);
        vector2_t normalized_base_position = px_character_base_position;
        normalized_base_position /= vector2_t((float)resolution.width, (float)resolution.height);
        normalized_base_position *= 2.0f;
        normalized_base_position -= vector2_t(1.0f);
        vector2_t normalized_size = (px_character_size / vector2_t((float)resolution.width, (float)resolution.height)) * 2.0f;
        vector2_t adjust = vector2_t(0.0f, -normalized_size.y);
        
        vector2_t current_uvs = font_character_data->uvs_base;
        current_uvs.y = 1.0f - current_uvs.y;
        
        push_textured_vertex({normalized_base_position + adjust, current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + vector2_t(0.0f, font_character_data->uvs_size.y);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + vector2_t(0.0f, normalized_size.y), current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + vector2_t(font_character_data->uvs_size.x, 0.0f);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + vector2_t(normalized_size.x, 0.0f), current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + vector2_t(0.0f, font_character_data->uvs_size.y);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + vector2_t(0.0f, normalized_size.y), current_uvs, color});

        current_uvs = font_character_data->uvs_base + vector2_t(font_character_data->uvs_size.x, 0.0f);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + vector2_t(normalized_size.x, 0.0f), current_uvs, color});

        current_uvs = font_character_data->uvs_base + font_character_data->uvs_size;
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + normalized_size, current_uvs, color});

        px_cursor_position += ivector2_t(px_char_width, 0.0f);

        ++character;
        ++chars_since_new_line;
        if (chars_since_new_line % text->chars_per_line == 0) {
            px_cursor_position.y -= px_char_height;
            px_cursor_position.x = (uint32_t)(text->x_start + box->px_position.ix);
        }
    }
}

#define BLINK_SPEED 2.0f 

void push_ui_input_text(
    bool render_cursor,
    bool secret,
    uint32_t cursor_color,
    struct ui_input_text_t *input) {
    VkExtent2D resolution = r_swapchain_extent();
    
    if (input->fade_in_or_out) {
        input->cursor_fade -= (int32_t)(BLINK_SPEED * surface_delta_time() * 1000.0f);
        if (input->cursor_fade < 0.0f) {
            input->cursor_fade = 0;
            input->fade_in_or_out ^= 1;
        }
        //cursor_color >>= 8;
        //cursor_color <<= 8;
        uint32_t cursor_alpha = cursor_color & 0xFF;
        uint32_t alpha = uint32_t(((float)input->cursor_fade / 255.0f) * ((float)(cursor_alpha)));
        cursor_color |= alpha;
    }
    else {
        input->cursor_fade += (int32_t)(BLINK_SPEED * surface_delta_time() * 1000.0f);
        if (input->cursor_fade > 1.0f) {
            input->cursor_fade = 1;
            input->fade_in_or_out ^= 1;
        }
        
        //cursor_color >>= 8;
        //cursor_color <<= 8;
        uint32_t cursor_alpha = cursor_color & 0xFF;
        uint32_t alpha = uint32_t(((float)input->cursor_fade) * ((float)(cursor_alpha)));
        cursor_color |= alpha;
    }
    
    // Push input text
    push_ui_text(&input->text, secret);
    // Push cursor quad
    if (render_cursor) {
        ui_box_t *box = input->text.dst_box;
        ui_text_t *text = &input->text;

        float magic = 1.0f;
        uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
        uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
        px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
        uint32_t px_char_height = (uint32_t)((text->line_height * (float)px_char_width) * magic);
        ivector2_t px_cursor_start = get_px_cursor_position(box, &input->text, resolution);
        ivector2_t px_cursor_position = px_cursor_start + ivector2_t(px_char_width, 0.0f) * (int32_t)input->cursor_position;
        
        vector2_t px_cursor_size = vector2_t((float)px_char_width, (float)px_char_height);
        vector2_t px_cursor_base_position =  vector2_t(px_cursor_position) + vector2_t(vector2_t(0.0f, 0.0f));
        vector2_t normalized_cursor_position = px_cursor_base_position;
        normalized_cursor_position /= vector2_t((float)resolution.width, (float)resolution.height);
        normalized_cursor_position *= 2.0f;
        normalized_cursor_position -= vector2_t(1.0f);
        vector2_t normalized_size = (px_cursor_size / vector2_t((float)resolution.width, (float)resolution.height)) * 2.0f;
        vector2_t adjust = vector2_t(0.0f, -normalized_size.y);

        push_colored_vertex({normalized_cursor_position + adjust, cursor_color});
        push_colored_vertex({normalized_cursor_position + adjust + vector2_t(0.0f, normalized_size.y), cursor_color});
        push_colored_vertex({normalized_cursor_position + adjust + vector2_t(normalized_size.x, 0.0f), cursor_color});
        push_colored_vertex({normalized_cursor_position + adjust + vector2_t(0.0f, normalized_size.y), cursor_color});
        push_colored_vertex({normalized_cursor_position + adjust + vector2_t(normalized_size.x, 0.0f), cursor_color});
        push_colored_vertex({normalized_cursor_position + adjust + normalized_size, cursor_color});
    }
}

void clear_ui_color_containers() {
    colored_list.vertex_count = 0;
}

void sync_gpu_with_colored_vertex_list(
    VkCommandBuffer command_buffer) {
    if (colored_list.vertex_count) {
        update_gpu_buffer(
            command_buffer,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0,
            sizeof(gui_colored_vertex_t) * colored_list.vertex_count,
            colored_list.vertices,
            &colored_list.vtx_buffer);
    }
}

void render_colored_quads(
    VkCommandBuffer command_buffer) {
    if (colored_list.vertex_count) {
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
            colored_quads_shader.pipeline);

        VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(
            command_buffer,
            0,
            1,
            &colored_list.vtx_buffer.buffer,
            &zero);

        vkCmdDraw(
            command_buffer,
            colored_list.vertex_count,
            1,
            0,
            0);
    }
}

void render_submitted_ui(
    VkCommandBuffer transfer_command_buffer,
    VkCommandBuffer ui_command_buffer) {
    sync_gpu_with_colored_vertex_list(transfer_command_buffer);
    sync_gpu_with_textured_vertex_list(transfer_command_buffer);

    render_colored_quads(ui_command_buffer);
    render_textured_quads(ui_command_buffer);

    // Clear containers
    clear_texture_list_containers();
    clear_ui_color_containers();
}
