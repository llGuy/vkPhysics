#pragma once

#include <vk.hpp>
#include <vkph_state.hpp>
#include <vkph_events.hpp>

// Scene where we are in game
void sc_play_init(vkph::listener_t listener);
void sc_bind_play(vkph::state_t *state);
void sc_play_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer shadow,
    vkph::state_t *state);
void sc_handle_play_event(void *object, vkph::event_t *event);
bool sc_is_in_first_person();
