#include "engine.hpp"

struct event_system_t {
    listener_callback_t callbacks[MAX_LISTENERS] = {};
    
    uint32_t listener_count = 0;
    void *listener_objects[MAX_LISTENERS] = {};

    listener_subscriptions_t subscriptions[event_type_t::INVALID_EVENT_TYPE] = {};

    uint32_t pending_event_count = 0;
    event_t pending_events[MAX_EVENTS] = {};
};

static event_system_t events;

listener_t set_listener_callback(listener_callback_t callback, void *object) {
    listener_t id = events.listener_count++;
    events.callbacks[id] = callback;
    events.listener_objects[id] = object;

    return id;
}

void subscribe(event_type_t type, listener_t listener) {
    events.subscriptions[type].listeners[events.subscriptions[type].count++] = listener;
}

void submit_event(event_type_t type, void *data) {
    events.pending_events[events.pending_event_count++] = event_t{ type, data };
}

void dispatch_events() {
    for (uint32_t i = 0; i < events.pending_event_count; ++i) {
        event_t *current_event = &events.pending_events[i];
        listener_subscriptions_t *subscription = &events.subscriptions[current_event->type];

        for (uint32_t event_listener = 0; event_listener < subscription->count; ++event_listener) {
            listener_callback_t callback = events.callbacks[subscription->listeners[event_listener]];
            void *object = events.listener_objects[event_listener];

            (*callback)(object, current_event);
        }
    }

    events.pending_event_count = 0;
}
