#pragma once

#include "meta.hpp"
#include "tools.hpp"

// Engine events
#define MAX_EVENTS 60
#define MAX_LISTENERS 20

typedef uint32_t listener_t;

enum event_type_t {
    // When window gets resurfaced (various parts of the system need to know - e.g. UI code)
    ET_RESIZE_SURFACE,
    // Shuts down the program
    ET_CLOSED_WINDOW,
    // When the user presses a join server button, needs to notify network code to send a join request to game server
    ET_REQUEST_TO_JOIN_SERVER,
    // When game server replies, submit the ET_ENTER_SERVER event to notify for instance the world code to initialise the world
    ET_ENTER_SERVER,
    // This happens when the user presses the spawn button (with the "play" icon in the game menu)
    ET_SPAWN,
    // When a new player joins the game server, client will receive a packet saying a new player joined
    // The client program will submit this event to add the player to the list of players to interpolate
    // This is the same thing with the server program
    ET_NEW_PLAYER,
    // Once the user id and the user tag of the current user is retrieved, this will be triggered,
    // Causing the client program to send an automatic login request to the meta server
    ET_START_CLIENT,
    // Once the server is and server tag of the current server program is retrieved, this will be triggered
    // Causing the server program to send an automatic packet to the meta server notifying it of its online status
    ET_START_SERVER,
    // This event is a very general event that may cause different things to happen depending on which scene is
    // Bound. It happens when the escape key is pressed (duh)
    ET_PRESSED_ESCAPE,
    // Notifies the network module to tell the game server that we're getting out of here
    ET_LEAVE_SERVER,
    // When another player leaves, this will notify the client (same thing with the server program)
    // To get rid of the player data in the world code
    ET_PLAYER_DISCONNECTED,
    // When user presses the refresh button on main menu (for available servers)
    // This event will notify the network module to send a request to the meta server requesting
    // All the available (online) game servers
    ET_REQUEST_REFRESH_SERVER_PAGE,
    // When the client receives the response from the meta server, this event will notify the UI module
    // To update what is on the main menu page
    ET_RECEIVED_AVAILABLE_SERVERS,
    // This is an event which will cause a fade effect to begin. Fade effects are used throughout the lifetime
    // Of the game. They are used to transition between different scenes. When the program reaches the middle
    // Of a fade effect, the window will be all black. Here, it is possible to trigger another event.
    // Often what happens, is all the ACTUAL work gets done when the screen is black (e.g. leaving
    // The server, and going back to the main menu) so that there isn't a really blunt cut
    ET_BEGIN_FADE, // With data, can be in / out
    // In case some code has to be executed when the fade effect has finished
    ET_FADE_FINISHED, // Just so that game can know when to do some sort of transition or something...

    ET_EXIT_SCENE,

    ET_ENTER_MAIN_MENU_SCENE,
    // These two purely affect the main menu scene
    ET_ENTER_GAME_PLAY_SCENE,
    ET_ENTER_MAP_CREATOR_SCENE,

    ET_PAUSE,
    ET_UNPAUSE,
    ET_LOCAL_PLAYER_DIED,

    // ----------------------------- REDUNDANT FOR NOW (not doing any AI testing)
    ET_BEGIN_AI_TRAINING,
    ET_FINISH_GENERATION,
    ET_RESET_AI_ARENA,
    // -----------------------------

    // When the username hasn't registered, request a username and password
    ET_REQUEST_USER_INFORMATION,

    // When the user clicks on the sign up button to register a username and password, request gets sent
    // To the meta server - client waits for response
    ET_ATTEMPT_SIGN_UP,
    // Meta server has responded with the sign up success
    ET_SIGN_UP_SUCCESS,
    // Similar to above, except it's for logging in
    ET_ATTEMPT_LOGIN,
    ET_LOGIN_SUCCESS,

    ET_META_REQUEST_ERROR,

    // Event that is used to begin building maps
    ET_BEGIN_MAP_EDITING,
    ET_CREATE_NEW_MAP,
    ET_DONT_CREATE_NEW_MAP,

    ET_MAP_EDITOR_CHOSE_COLOR,

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
    const char *client_name;
    uint16_t client_id;
    // A bunch of shit (more will come)
    // (name and client_id are already in client_data)
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;

    uint32_t flags;

    vector3_t next_random_spawn_position;
};

struct event_enter_server_t {
    uint16_t local_client_id;

    uint32_t info_count;
    player_init_info_t *infos;

    // Will need to have other stuff
};

struct event_spawn_t {
    uint32_t client_id;
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

struct fade_trigger_t {
    event_type_t trigger_type;
    void *next_event_data;
};

struct event_begin_fade_effect_t {
    float dest_value;
    float duration;

    // Can make the fade effect trigger another event (like opening a menu or something)
    uint32_t trigger_count = 0;
    fade_trigger_t triggers[5];
    
    bool fade_back;
};

enum ai_training_session_t {
    ATS_WALKING,
    ATS_ROLLING,
};

struct event_begin_ai_training_t {
    ai_training_session_t session_type;
};

struct event_t {
    event_type_t type;

    // Likely to be allocated on free list allocator
    void *data;
};

struct event_attempt_sign_up_t {
    const char *username;
    const char *password;
};

struct event_attempt_login_t {
    const char *username;
    const char *password;
};

struct event_meta_request_error_t {
    request_error_t error_type;
};

struct event_enter_map_creator_t {
    const char *map_path;
    const char *map_name;
};

struct event_map_editor_chose_color_t {
    uint8_t r, g, b;
};

typedef void(*listener_callback_t)(
    void *object,
    event_t *,
    struct event_submissions_t *event_system);

struct listener_subscriptions_t {
    uint32_t count = 0;
    uint32_t listeners[MAX_LISTENERS] = {};
    uint8_t listener_table[MAX_LISTENERS] = {};
};

struct event_submissions_t {
    listener_callback_t callbacks[MAX_LISTENERS] = {};
    
    uint32_t listener_count = 0;
    void *listener_objects[MAX_LISTENERS] = {};

    listener_subscriptions_t subscriptions[ET_INVALID_EVENT_TYPE] = {};

    uint32_t pending_event_count = 0;
    event_t pending_events[MAX_EVENTS] = {};
};

// Makes sure no duplicates occur
void begin_listener_subscription(listener_t listener, event_submissions_t *events);
void end_listener_subscriptions(listener_t listener, event_submissions_t *events);

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
