#pragma once

#include "tools.hpp"

// Engine events
#define MAX_EVENTS 40
#define MAX_LISTENERS 20

typedef uint32_t listener_t;

enum event_type_t {
    ET_RESIZE_SURFACE,
    ET_CLOSED_WINDOW,
    ET_REQUEST_TO_JOIN_SERVER,
    ET_ENTER_SERVER,
    ET_NEW_PLAYER,
    ET_START_CLIENT,
    ET_START_SERVER,
    ET_PRESSED_ESCAPE,
    ET_LEAVE_SERVER,
    ET_PLAYER_DISCONNECTED,
    ET_STARTED_RECEIVING_INITIAL_CHUNK_DATA,
    ET_FINISHED_RECEIVING_INITIAL_CHUNK_DATA,
    ET_SET_CHUNK_HISTORY_TRACKER,
    ET_REQUEST_REFRESH_SERVER_PAGE,
    ET_INVALID_EVENT_TYPE
};

struct event_surface_resize_t {
    uint32_t width;
    uint32_t height;
};

struct event_start_client_t {
    const char *client_name;
};

struct event_start_server_t {
    // Clients will see these names with looking at servers list
    const char *server_name;
};

struct event_data_request_to_join_server_t {
    // Program can either provide server name or raw ip address: both can work
    // Value copy of game server information
    const char *server_name;
    const char *ip_address;
};

struct player_init_info_t {
    struct client_t *client_data;
    // A bunch of shit (more will come)
    // (name and client_id are already in client_data)
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;
    bool is_local;
};

struct event_enter_server_t {
    uint16_t local_client_id;

    uint32_t info_count;
    player_init_info_t *infos;

    // Will need to have other stuff
};

struct event_new_player_t {
    player_init_info_t info;
};

struct event_player_disconnected_t {
    uint16_t client_id;
};

struct event_chunk_voxel_packet_t {
    struct packet_chunk_voxels_t *packet;
};

struct event_set_chunk_history_tracker_t {
    bool value;
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
