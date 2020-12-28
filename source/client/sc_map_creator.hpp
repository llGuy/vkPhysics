#pragma once

#include <vkph_events.hpp>
#include <vkph_state.hpp>
#include <vk.hpp>

// Scene where we are building a map
void sc_map_creator_init(vkph::listener_t listener);
void sc_bind_map_creator(vkph::state_t *state);
void sc_map_creator_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer render_shadow,
    vkph::state_t *state);
void sc_handle_map_creator_event(void *object, vkph::event_t *event);
