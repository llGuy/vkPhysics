#pragma once

#include <common/event.hpp>

void wd_subscribe_to_events(listener_t world_listener, event_submissions_t *events);
void wd_world_event_listener(void *object, event_t *event, event_submissions_t *events);
