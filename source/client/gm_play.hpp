#pragma once

#include <renderer/renderer.hpp>

// Game mode where we are in game
void gm_play_init();
void gm_bind_play();
void gm_play_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, struct event_submissions_t *events);
