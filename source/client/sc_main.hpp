#pragma once

#include <vkph_events.hpp>
#include <vkph_state.hpp>
#include <vk.hpp>

// Scene where we are in the main menu
void sc_main_init(vkph::listener_t listener);
void sc_bind_main();
void sc_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, vkph::state_t *state);
void sc_handle_main_event(void *object, vkph::event_t *event);
