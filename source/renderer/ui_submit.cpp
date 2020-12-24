#include "vk_buffer.hpp"
#include "vk_shader.hpp"
#include "ui_submit.hpp"
#include "vk_present.hpp"
#include "app_context.hpp"

#include <ui.hpp>
#include <common/log.hpp>
#include <common/math.hpp>

namespace ui {

struct vertex_section_t {
    uint32_t section_start, section_size;
};

static struct textured_vertex_render_list_t {
    uint32_t section_count = 0;
    // Index where there is a transition to the next texture to bind for the following vertices
    vertex_section_t sections[MAX_TEXTURES_IN_RENDER_LIST] = {};
    // Corresponds to the same index as the index of the current section
    VkDescriptorSet textures[MAX_TEXTURES_IN_RENDER_LIST] = {};

    uint32_t vertex_count;
    textured_vertex_t vertices[MAX_QUADS_TO_RENDER * 6];

    vk::gpu_buffer_t vtx_buffer;
} textured_list;

static struct color_vertex_render_list_t {
    uint32_t vertex_count;
    color_vertex_t vertices[MAX_QUADS_TO_RENDER * 6];

    vk::gpu_buffer_t vtx_buffer;
} color_list;

static vk::shader_t textured_quads_shader;
static vk::shader_t color_quads_shader;

static void s_color_shader_init() {
    vk::shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = LN_MALLOC(VkVertexInputBindingDescription, 1);
    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(color_vertex_t);

    binding_info.attribute_count = 2;
    binding_info.attribute_descriptions = LN_MALLOC(VkVertexInputAttributeDescription, 2);
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    binding_info.attribute_descriptions[1].location = 1;
    binding_info.attribute_descriptions[1].binding = 0;
    binding_info.attribute_descriptions[1].format = VK_FORMAT_R32_UINT;
    binding_info.attribute_descriptions[1].offset = sizeof(::vector2_t);

    const char *color_shader_paths[] = {
        "shaders/SPV/uiquad.vert.spv",
        "shaders/SPV/uiquad.frag.spv"
    };

    color_quads_shader.init_as_ui_shader(
        &binding_info,
        0,
        NULL,
        0,
        color_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        vk::AB_ONE_MINUS_SRC_ALPHA);
}

static void s_textured_shader_init() {
    vk::shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = LN_MALLOC(VkVertexInputBindingDescription, 1);
    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(textured_vertex_t);

    binding_info.attribute_count = 3;
    binding_info.attribute_descriptions = LN_MALLOC(VkVertexInputAttributeDescription, 3);
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    binding_info.attribute_descriptions[1].location = 1;
    binding_info.attribute_descriptions[1].binding = 0;
    binding_info.attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    binding_info.attribute_descriptions[1].offset = sizeof(::vector2_t);

    binding_info.attribute_descriptions[2].location = 2;
    binding_info.attribute_descriptions[2].binding = 0;
    binding_info.attribute_descriptions[2].format = VK_FORMAT_R32_UINT;
    binding_info.attribute_descriptions[2].offset = sizeof(::vector2_t) + sizeof(::vector2_t);

    const char *texture_shader_paths[] = {
        "shaders/SPV/uiquadtex.vert.spv",
        "shaders/SPV/uiquadtex.frag.spv"
    };

    VkDescriptorType texture_descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    textured_quads_shader.init_as_ui_shader(
        &binding_info,
        0,
        &texture_descriptor_type,
        1,
        texture_shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        vk::AB_ONE_MINUS_SRC_ALPHA);
}

void init_submission() {
    s_color_shader_init();
    s_textured_shader_init();

    color_list.vtx_buffer.init(
        MAX_QUADS_TO_RENDER * 6 * sizeof(color_vertex_t),
        color_list.vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    textured_list.vtx_buffer.init(
        MAX_QUADS_TO_RENDER * 6 * sizeof(textured_vertex_t),
        textured_list.vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void mark_ui_textured_section(VkDescriptorSet set) {
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

void push_textured_vertex(const textured_vertex_t &vertex) {
    textured_list.vertices[textured_list.vertex_count++] = vertex;
    textured_list.sections[textured_list.section_count - 1].section_size += 1;
}

void push_textured_box(const box_t *box, ::vector2_t *uvs) {
    ::vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    ::vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;
    
    push_textured_vertex({normalized_base_position, uvs[0], box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), uvs[1], box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), uvs[2], box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), uvs[3], box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), uvs[4], box->color});
    push_textured_vertex({normalized_base_position + normalized_size, uvs[5], box->color});
}

void push_textured_box(const box_t *box) {
    ::vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    ::vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;
    
    push_textured_vertex({normalized_base_position, ::vector2_t(0.0f, 1.0f), box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), ::vector2_t(0.0f), box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), ::vector2_t(1.0f), box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), ::vector2_t(0.0f), box->color});
    push_textured_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), ::vector2_t(1.0f), box->color});
    push_textured_vertex({normalized_base_position + normalized_size, ::vector2_t(1.0f, 0.0f), box->color});
}

void clear_texture_list_containers() {
    textured_list.section_count = 0;
    textured_list.vertex_count = 0;
}

void sync_gpu_with_textured_vertex_list(VkCommandBuffer cmdbuf) {
    if (textured_list.vertex_count) {
        textured_list.vtx_buffer.update(
            cmdbuf,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0,
            sizeof(textured_vertex_t) * textured_list.vertex_count,
            textured_list.vertices);

        if (sizeof(textured_vertex_t) * textured_list.vertex_count > 65536) {
            LOG_ERROR("WE HAVE A BIG PROBLEM\n");
        }
    }
}

void push_color_vertex(const color_vertex_t &vertex) {
    color_list.vertices[color_list.vertex_count++] = vertex;
}

void push_color_box(const box_t *box) {
    ::vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    ::vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    push_color_vertex({normalized_base_position, box->color});
    push_color_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), box->color});
    push_color_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), box->color});
    push_color_vertex({normalized_base_position + ::vector2_t(0.0f, normalized_size.y), box->color});
    push_color_vertex({normalized_base_position + ::vector2_t(normalized_size.x, 0.0f), box->color});
    push_color_vertex({normalized_base_position + normalized_size, box->color});
}

void push_reversed_color_box(const box_t *box, const ::vector2_t &size) {
    ::vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    ::vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    normalized_base_position += size;

    push_color_vertex({normalized_base_position, box->color});
    push_color_vertex({normalized_base_position - ::vector2_t(0.0f, normalized_size.y), box->color});
    push_color_vertex({normalized_base_position - ::vector2_t(normalized_size.x, 0.0f), box->color});
    push_color_vertex({normalized_base_position - ::vector2_t(0.0f, normalized_size.y), box->color});
    push_color_vertex({normalized_base_position - ::vector2_t(normalized_size.x, 0.0f), box->color});
    push_color_vertex({normalized_base_position - normalized_size, box->color});
}

static ivector2_t s_get_px_cursor_position(
    box_t *box,
    text_t *text,
    const VkExtent2D &resolution) {
    uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
    uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
    px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
    uint32_t px_char_height = (uint32_t)(text->line_height * (float)px_char_width);
    
    ivector2_t px_cursor_position;
    switch(text->relative_to) {
    case text_t::font_stream_box_relative_to_t::TOP: {
        uint32_t px_box_top = box->px_position.iy + box->px_current_size.iy;
        px_cursor_position = ivector2_t(x_start + box->px_position.ix, px_box_top - (uint32_t)(text->y_start * (float)px_char_width));
        break;
    }
    case text_t::font_stream_box_relative_to_t::BOTTOM: {
        uint32_t px_box_bottom = box->px_position.iy;
        px_cursor_position = ivector2_t(x_start + box->px_position.ix, px_box_bottom + ((uint32_t)(text->y_start * (float)(px_char_width)) + px_char_height));
        break;
    }
    }
    return(px_cursor_position);
}

void push_text(text_t *text, bool secret) {
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();
    
    VkExtent2D resolution = {swapchain_info.width, swapchain_info.height};

    box_t *box = text->dst_box;

    uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
    uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
    px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
    uint32_t px_char_height = (uint32_t)(text->line_height * (float)px_char_width);
    
    ivector2_t px_cursor_position = s_get_px_cursor_position(box, text, resolution);

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
        
        ::vector2_t px_character_size = ::vector2_t(font_character_data->display_size) * (float)px_char_width;
        ::vector2_t px_character_base_position =  ::vector2_t(px_cursor_position) + ::vector2_t(font_character_data->offset) * (float)px_char_width;
        ::vector2_t normalized_base_position = px_character_base_position;
        normalized_base_position /= ::vector2_t((float)resolution.width, (float)resolution.height);
        normalized_base_position *= 2.0f;
        normalized_base_position -= ::vector2_t(1.0f);
        ::vector2_t normalized_size = (px_character_size / ::vector2_t((float)resolution.width, (float)resolution.height)) * 2.0f;
        ::vector2_t adjust = ::vector2_t(0.0f, -normalized_size.y);
        
        ::vector2_t current_uvs = font_character_data->uvs_base;
        current_uvs.y = 1.0f - current_uvs.y;
        
        push_textured_vertex({normalized_base_position + adjust, current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + ::vector2_t(0.0f, font_character_data->uvs_size.y);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + ::vector2_t(0.0f, normalized_size.y), current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + ::vector2_t(font_character_data->uvs_size.x, 0.0f);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + ::vector2_t(normalized_size.x, 0.0f), current_uvs, color});
        
        current_uvs = font_character_data->uvs_base + ::vector2_t(0.0f, font_character_data->uvs_size.y);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + ::vector2_t(0.0f, normalized_size.y), current_uvs, color});

        current_uvs = font_character_data->uvs_base + ::vector2_t(font_character_data->uvs_size.x, 0.0f);
        current_uvs.y = 1.0f - current_uvs.y;
        push_textured_vertex({normalized_base_position + adjust + ::vector2_t(normalized_size.x, 0.0f), current_uvs, color});

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

static constexpr uint32_t BLINK_SPEED = 2.0f;

void push_ui_input_text(
    bool render_cursor,
    bool secret,
    uint32_t cursor_color,
    input_text_t *input) {
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();
    VkExtent2D resolution = {swapchain_info.width, swapchain_info.height};
    
    if (input->fade_in_or_out) {
        input->cursor_fade -= (int32_t)(BLINK_SPEED * app::g_delta_time * 1000.0f);
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
        input->cursor_fade += (int32_t)(BLINK_SPEED * app::g_delta_time * 1000.0f);
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
    push_text(&input->text, secret);
    // Push cursor quad
    if (render_cursor) {
        box_t *box = input->text.dst_box;
        text_t *text = &input->text;

        float magic = 1.0f;
        uint32_t px_char_width = (box->px_current_size.ix) / text->chars_per_line;
        uint32_t x_start = (uint32_t)((float)px_char_width * text->x_start);
        px_char_width = (box->px_current_size.ix - 2 * x_start) / text->chars_per_line;
        uint32_t px_char_height = (uint32_t)((text->line_height * (float)px_char_width) * magic);
        ivector2_t px_cursor_start = s_get_px_cursor_position(box, &input->text, resolution);
        ivector2_t px_cursor_position = px_cursor_start + ivector2_t(px_char_width, 0.0f) * (int32_t)input->cursor_position;
        
        ::vector2_t px_cursor_size = ::vector2_t((float)px_char_width, (float)px_char_height);
        ::vector2_t px_cursor_base_position =  ::vector2_t(px_cursor_position) + ::vector2_t(0.0f, 0.0f);
        ::vector2_t normalized_cursor_position = px_cursor_base_position;
        normalized_cursor_position /= ::vector2_t((float)resolution.width, (float)resolution.height);
        normalized_cursor_position *= 2.0f;
        normalized_cursor_position -= ::vector2_t(1.0f);
        ::vector2_t normalized_size = (px_cursor_size / ::vector2_t((float)resolution.width, (float)resolution.height)) * 2.0f;
        ::vector2_t adjust = ::vector2_t(0.0f, -normalized_size.y);

        push_color_vertex({normalized_cursor_position + adjust, cursor_color});
        push_color_vertex({normalized_cursor_position + adjust + ::vector2_t(0.0f, normalized_size.y), cursor_color});
        push_color_vertex({normalized_cursor_position + adjust + ::vector2_t(normalized_size.x, 0.0f), cursor_color});
        push_color_vertex({normalized_cursor_position + adjust + ::vector2_t(0.0f, normalized_size.y), cursor_color});
        push_color_vertex({normalized_cursor_position + adjust + ::vector2_t(normalized_size.x, 0.0f), cursor_color});
        push_color_vertex({normalized_cursor_position + adjust + normalized_size, cursor_color});
    }
}

void clear_ui_color_containers() {
    color_list.vertex_count = 0;
}

void sync_gpu_with_color_vertex_list(VkCommandBuffer cmdbuf) {
    if (color_list.vertex_count) {
        color_list.vtx_buffer.update(
            cmdbuf,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0,
            sizeof(color_vertex_t) * color_list.vertex_count,
            color_list.vertices);
    }
}

void s_render_textured_quads(VkCommandBuffer cmdbuf, const VkExtent2D *extent) {
    if (textured_list.vertex_count) {
        VkViewport viewport = {};
        viewport.width = (float)extent->width;
        viewport.height = (float)extent->height;
        viewport.maxDepth = 1;
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

        VkRect2D rect = {};
        rect.extent = *extent;
        vkCmdSetScissor(cmdbuf, 0, 1, &rect);

        vkCmdBindPipeline(
            cmdbuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            textured_quads_shader.pipeline);

        // Loop through each section, bind texture, and render the quads from the section
        for (uint32_t current_section = 0; current_section < textured_list.section_count; ++current_section) {
            vertex_section_t *section = &textured_list.sections[current_section];
                
            if (section->section_size) {
                vkCmdBindDescriptorSets(
                    cmdbuf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    textured_quads_shader.layout,
                    0,
                    1,
                    &textured_list.textures[current_section],
                    0,
                    NULL);

                VkDeviceSize zero = 0;
                vkCmdBindVertexBuffers(
                    cmdbuf,
                    0,
                    1,
                    &textured_list.vtx_buffer.buffer,
                    &zero);

                vkCmdDraw(
                    cmdbuf,
                    section->section_size,
                    1,
                    section->section_start,
                    0);
            }
        }
    }
}

void s_render_color_quads(VkCommandBuffer cmdbuf, const VkExtent2D *extent) {
    if (color_list.vertex_count) {
        VkViewport viewport = {};
        viewport.width = (float)extent->width;
        viewport.height = (float)extent->height;
        viewport.maxDepth = 1;
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

        VkRect2D rect = {};
        rect.extent = *extent;
        vkCmdSetScissor(cmdbuf, 0, 1, &rect);

        vkCmdBindPipeline(
            cmdbuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            color_quads_shader.pipeline);

        VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(
            cmdbuf,
            0,
            1,
            &color_list.vtx_buffer.buffer,
            &zero);

        vkCmdDraw(
            cmdbuf,
            color_list.vertex_count,
            1,
            0,
            0);
    }
}

void render_submitted_ui(VkCommandBuffer transfer_cmdbuf, VkCommandBuffer ui_cmdbuf) {
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();
    VkExtent2D extent = {swapchain_info.width, swapchain_info.height};

    sync_gpu_with_color_vertex_list(transfer_cmdbuf);
    sync_gpu_with_textured_vertex_list(transfer_cmdbuf);

    s_render_color_quads(ui_cmdbuf, &extent);
    s_render_textured_quads(ui_cmdbuf, &extent);

    // Clear containers
    clear_texture_list_containers();
    clear_ui_color_containers();
}

}
