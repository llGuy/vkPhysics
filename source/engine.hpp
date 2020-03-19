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
    ET_RESIZE_SURFACE,
    ET_REQUEST_TO_JOIN_SERVER,
    ET_ENTER_SERVER_WORLD,
    ET_LAUNCH_MAP_EDITOR,
    ET_OPEN_MENU,
    ET_EXIT_MENU,
    ET_OPEN_CONSOLE,
    ET_EXIT_CONSOLE,
    ET_REQUEST_USERNAME,
    ET_ENTERED_USERNAME,
    ET_CACHE_PLAYER_COMMAND,
    ET_INVALID_EVENT_TYPE
};

struct event_surface_resize_t {
    uint32_t width;
    uint32_t height;
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

listener_t set_listener_callback(
    listener_callback_t callback,
    void *object);

void subscribe_to_event(
    event_type_t type,
    listener_t listener);

void submit_event(
    event_type_t type,
    void *data);

void dispatch_events();
