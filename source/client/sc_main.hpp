#pragma once

#include <common/event.hpp>
#include <vk.hpp>

// Scene where we are in the main menu
void sc_main_init(listener_t listener, event_submissions_t *events);
void sc_bind_main();
void sc_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events);
void sc_handle_main_event(void *object, event_t *event, event_submissions_t *events);
