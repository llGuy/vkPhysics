#pragma once

#include <common/event.hpp>

void cl_subscribe_to_events(listener_t core_listener, event_submissions_t *events);
void cl_game_event_listener(void *object, event_t *event, event_submissions_t *events);
