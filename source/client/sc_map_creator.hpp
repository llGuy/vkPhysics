#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>

// Scene where we are building a map
void sc_map_creator_init(listener_t listener, event_submissions_t *events);
void sc_bind_map_creator();
void sc_map_creator_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events);
void sc_handle_map_creator_event(void *object, event_t *event, event_submissions_t *events);
