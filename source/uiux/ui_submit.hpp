#pragma once

#include "ui_font.hpp"

#include <math.hpp>
#include <vulkan/vulkan.h>

namespace ui {

/*
  Basically initialises the only global state of UI that is needed.
 */
void init_submission();

/*
  Make sure to call this before passing the ui secondary command buffer
  to the main renderer.
 */
void render_submitted_ui(VkCommandBuffer transfer_cmdbuf, VkCommandBuffer ui_cmdbuf);

struct color_vertex_t {
    ::vector2_t position;
    uint32_t color;
};

struct textured_vertex_t {
    ::vector2_t position;
    ::vector2_t uvs;
    uint32_t color;
};

constexpr uint32_t MAX_TEXTURES_IN_RENDER_LIST = 25;
constexpr uint32_t MAX_QUADS_TO_RENDER = 1000;

void push_textured_vertex(const textured_vertex_t &vertex);
void clear_texture_list_containers();
void push_color_vertex(const color_vertex_t &vertex);
void clear_ui_color_containers();

void sync_gpu_with_textured_vertex_list(VkCommandBuffer cmdbuf);
void sync_gpu_with_color_vertex_list(VkCommandBuffer cmdbuf);

/*
  For submission that the client program will most likely use.
 */
void mark_ui_textured_section(VkDescriptorSet set);
void push_textured_box(const struct box_t *box, ::vector2_t *uvs);
void push_textured_box(const struct box_t *box);
void push_color_box(const box_t *box);
void push_reversed_color_box(const box_t *box, const ::vector2_t &size);
// If secret == 1, this is a password (only '*' will show up)
void push_text(text_t *text, bool secret = 0);
void push_ui_input_text(bool render_cursor, bool secret, uint32_t cursor_color, input_text_t *input);

}
