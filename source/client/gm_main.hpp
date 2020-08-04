#pragma once

#include <renderer/renderer.hpp>

// Game mode where we are in the main menu
void gm_main_init();
void gm_bind_main();
void gm_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, struct event_submissions_t *events);
