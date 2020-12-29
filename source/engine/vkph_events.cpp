#include "vkph_events.hpp"

#include <log.hpp>

namespace vkph {

struct listener_subscriptions_t {
    uint32_t count = 0;
    uint32_t listeners[MAX_LISTENERS] = {};
    uint8_t listener_table[MAX_LISTENERS] = {};
};

static struct {
    listener_callback_t callbacks[MAX_LISTENERS] = {};
    
    uint32_t listener_count = 0;
    void *listener_objects[MAX_LISTENERS] = {};

    listener_subscriptions_t subscriptions[ET_INVALID_EVENT_TYPE] = {};

    uint32_t pending_event_count = 0;
    event_t pending_events[MAX_EVENTS] = {};
} events = {};

listener_t set_listener_callback(listener_callback_t callback, void *object) {
    listener_t id = events.listener_count++;
    events.callbacks[id] = callback;
    events.listener_objects[id] = object;

    return id;
}

void subscribe_to_event(event_type_t type, listener_t listener) {
    // Check that there wasn't already a subscription to this events, from this listener
    if (!(events.subscriptions[type].listener_table[listener])) {
        events.subscriptions[type].listeners[events.subscriptions[type].count++] = listener;
        events.subscriptions[type].listener_table[listener] = 1;
    }
    else {
        LOG_ERROR("Listener already subscribed to this event\n");
    }
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
            void *object = events.listener_objects[subscription->listeners[event_listener]];

            (*callback)(object, current_event);
        }
    }

    events.pending_event_count = 0;
}

}
