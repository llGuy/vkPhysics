#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>

// Scene where we are in game
void sc_play_init(listener_t listener, event_submissions_t *events);
void sc_bind_play();
void sc_play_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events);
void sc_handle_play_event(void *object, event_t *event, event_submissions_t *events);
