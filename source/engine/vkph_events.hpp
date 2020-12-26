#pragma once

#include "vkph_event.hpp"

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

}
