#pragma once

#include <renderer/renderer.hpp>

void world_init();

void destory_world();

void handle_world_input();

void tick_world(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer);

eye_3d_info_t create_eye_info();
lighting_info_t create_lighting_info();