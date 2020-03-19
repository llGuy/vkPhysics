#pragma once

#include "tools.hpp"

enum game_init_flags_t {
    GIF_WINDOWED,
    GIF_NOT_WINDOWED,
    GIF_CLIENT,
    GIF_SERVER
};

struct game_init_data_t {
    uint32_t fl_pool_size;
    uint32_t ln_pool_size;
    game_init_flags_t flags;
};

void game_main(
    game_init_data_t *game_init_data);

void run_game();

void finish_game();

float logic_delta_time();
float surface_delta_time();

// Engine events
#define MAX_EVENTS 20
#define MAX_LISTENERS 20

typedef uint32_t listener_t;

enum event_type_t {
    RESIZE_WINDOW,
    REQUEST_TO_JOIN_SERVER,
    ENTER_SERVER_WORLD,
    LAUNCH_MAP_EDITOR,
    OPEN_MENU,
    EXIT_MENU,
    OPEN_CONSOLE,
    EXIT_CONSOLE,
    REQUEST_USERNAME,
    ENTERED_USERNAME,
    CACHE_PLAYER_COMMAND,
    INVALID_EVENT_TYPE
};

struct event_data_request_to_join_server_t {
    const char *ip_address;
};

struct event_data_launch_map_editor_t {
    char *map_name;
};

struct event_t {
    event_type_t type;

    // Likely to be allocated on free list allocator
    void *data;
};

typedef void(*listener_callback_t)(void *object, event_t *);

struct listener_subscriptions_t {
    uint32_t count = 0;
    uint32_t listeners[MAX_LISTENERS] = {};
};

listener_t set_listener_callback(listener_callback_t callback, void *object);
void subscribe_to_event(event_type_t type, listener_t listener);
void submit_event(event_type_t type, void *data);
void dispatch_events();
