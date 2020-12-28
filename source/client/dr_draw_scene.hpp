#pragma once

#include <vk.hpp>
#include <vkph_state.hpp>

// Performs a GPU sync for all the data needed for rendering
// And renders
void dr_draw_game(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer shadow, vkph::state_t *state);
