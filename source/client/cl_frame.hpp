#pragma once

#include <vk.hpp>

namespace cl {

void init_frame_command_buffers();

struct frame_command_buffers_t {
    VkCommandBuffer render_cmdbuf;
    VkCommandBuffer render_shadow_cmdbuf;
    VkCommandBuffer transfer_cmdbuf;
    VkCommandBuffer ui_cmdbuf;
};

frame_command_buffers_t prepare_frame();
void finish_frame();

void enable_blur();
void enable_ssao();

void disable_blur();
void disable_ssao();

vk::frame_info_t *get_frame_info();

}
