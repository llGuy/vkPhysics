#pragma once

#include "tools.hpp"

// Engine events
#define MAX_EVENTS 20
#define MAX_LISTENERS 20

typedef uint32_t listener_t;

enum event_type_t {
    ET_RESIZE_SURFACE,
    ET_CLOSED_WINDOW,
    ET_REQUEST_TO_JOIN_SERVER,
    ET_ENTER_SERVER,
    ET_NEW_PLAYER_JOINED,
    ET_START_CLIENT,
    ET_START_SERVER,
    ET_PRESSED_ESCAPE,
    ET_INVALID_EVENT_TYPE
};

struct event_surface_resize_t {
    uint32_t width;
    uint32_t height;
};

struct event_data_request_to_join_server_t {
    const char *ip_address;
};

struct event_enter_server_t {
    uint16_t local_client_id;
};

struct event_new_player_joined_t {
    struct client_t *client_data;
};

struct event_t {
    event_type_t type;

    // Likely to be allocated on free list allocator
    void *data;
};

typedef void(*listener_callback_t)(
    void *object,
    event_t *);

struct listener_subscriptions_t {
    uint32_t count = 0;
    uint32_t listeners[MAX_LISTENERS] = {};
};

struct event_submissions_t {
    listener_callback_t callbacks[MAX_LISTENERS] = {};
    
    uint32_t listener_count = 0;
    void *listener_objects[MAX_LISTENERS] = {};

    listener_subscriptions_t subscriptions[ET_INVALID_EVENT_TYPE] = {};

    uint32_t pending_event_count = 0;
    event_t pending_events[MAX_EVENTS] = {};
};

// Setting object to NULL will just not bind callback to any data object (this would be used for structures with member functions)
listener_t set_listener_callback(
    listener_callback_t callback,
    void *object,
    event_submissions_t *event_data);

void subscribe_to_event(
    event_type_t type,
    listener_t listener,
    event_submissions_t *event_data);

void submit_event(
    event_type_t type,
    void *data,
    event_submissions_t *event_data);

void dispatch_events(
    event_submissions_t *event_data);
