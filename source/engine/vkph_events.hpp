#pragma once

#include "vkph_event.hpp"
#include <allocators.hpp>

namespace vkph {

typedef void(*listener_callback_t)(void *object, event_t *);

/* 
  Setting object to NULL will just not bind callback to any data object 
  (this would be used for structures with member functions).
*/
listener_t set_listener_callback(listener_callback_t callback, void *object);
void subscribe_to_event(event_type_t type, listener_t listener);
void submit_event(event_type_t type, void *data);
void dispatch_events();

template <typename T> T *make_event_data(const char *src) {
    T *event_data = flmalloc<T>(1);
    event_data->src = src;
    return event_data;
}

}

#define VKPH_MAKE_EVENT_DATA(type) make_event_data<type>(__FUNCTION__)
