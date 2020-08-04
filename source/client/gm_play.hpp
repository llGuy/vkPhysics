#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>

// Game mode where we are in game
void gm_play_init(listener_t listener, event_submissions_t *events);
void gm_bind_play();
void gm_play_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events);
void gm_handle_play_event(void *object, event_t *event, event_submissions_t *events);
