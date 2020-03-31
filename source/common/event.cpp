#include "event.hpp"

listener_t set_listener_callback(
    listener_callback_t callback,
    void *object,
    event_submissions_t *events) {
    listener_t id = events->listener_count++;
    events->callbacks[id] = callback;
    events->listener_objects[id] = object;

    return id;
}

void subscribe_to_event(
    event_type_t type,
    listener_t listener,
    event_submissions_t *events) {
    events->subscriptions[type].listeners[events->subscriptions[type].count++] = listener;
}

void submit_event(
    event_type_t type,
    void *data,
    event_submissions_t *events) {
    events->pending_events[events->pending_event_count++] = event_t{ type, data };
}

void dispatch_events(
    event_submissions_t *events) {
    for (uint32_t i = 0; i < events->pending_event_count; ++i) {
        event_t *current_event = &events->pending_events[i];
        listener_subscriptions_t *subscription = &events->subscriptions[current_event->type];

        for (uint32_t event_listener = 0; event_listener < subscription->count; ++event_listener) {
            listener_callback_t callback = events->callbacks[subscription->listeners[event_listener]];
            void *object = events->listener_objects[event_listener];

            (*callback)(object, current_event);
        }
    }

    events->pending_event_count = 0;
}
