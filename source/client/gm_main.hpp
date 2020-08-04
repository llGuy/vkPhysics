#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>

// Game mode where we are in the main menu
void gm_main_init(listener_t listener, event_submissions_t *events);
void gm_bind_main();
void gm_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events);
void gm_handle_main_event(void *object, event_t *event, event_submissions_t *events);
