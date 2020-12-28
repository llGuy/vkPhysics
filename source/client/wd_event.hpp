#pragma once

#include <vkph_events.hpp>
#include <vkph_state.hpp>

void wd_subscribe_to_events(vkph::listener_t world_listener);
void wd_world_event_listener(void *object, vkph::event_t *event);
