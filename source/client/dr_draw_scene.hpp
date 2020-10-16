#pragma once

#include <renderer/renderer.hpp>

// Performs a GPU sync for all the data needed for rendering
// And renders
void dr_draw_game(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer shadow);
