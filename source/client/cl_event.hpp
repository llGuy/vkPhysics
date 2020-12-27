#pragma once

#include <vkph_events.hpp>

void cl_subscribe_to_events(vkph::listener_t core_listener);
void cl_game_event_listener(void *object, vkph::event_t *event);
