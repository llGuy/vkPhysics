#pragma once

#include "input.hpp"
#include "renderer.hpp"

void world_init();

void handle_world_input();

void tick_world(
    VkCommandBuffer command_buffer);

eye_3d_info_t create_eye_info();
