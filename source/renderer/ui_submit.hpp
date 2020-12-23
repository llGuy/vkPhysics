#pragma once

#include <ui.hpp>
#include <common/math.hpp>
#include <vulkan/vulkan.h>

namespace ui {

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

}
