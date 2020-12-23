#pragma once

#include <vk.hpp>

void cl_command_buffers_init();

struct frame_command_buffers_t {
    VkCommandBuffer render_command_buffer;
    VkCommandBuffer render_shadow_command_buffer;
    VkCommandBuffer transfer_command_buffer;
    VkCommandBuffer ui_command_buffer;
};

frame_command_buffers_t cl_prepare_frame();
void cl_finish_frame();
