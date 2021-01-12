#pragma once

#include <stdint.h>

namespace vkph {

constexpr uint32_t MAX_EVENTS = 60;
constexpr uint32_t MAX_LISTENERS = 20;

using listener_t = uint32_t;

/*
  All the possible engine events. These events might have nothing to do with game code
  (for instance: rendering, or UIUX).
 */
enum event_type_t {
    /*
      Core = 0x0..
    */

    ET_CLOSED_WINDOW = 0x00,
    // This event is a very general event that may cause different things to happen depending on which scene is
    // Bound. It happens when the escape key is pressed (duh)
    ET_PRESSED_ESCAPE = 0x01,

    /*
      Rendering = 0x1..
    */
    ET_RESIZE_SURFACE = 0x10,
    // This is an event which will cause a fade effect to begin. Fade effects are used throughout the lifetime
    // Of the game. They are used to transition between different scenes. When the program reaches the middle
    // Of a fade effect, the window will be all black. Here, it is possible to trigger another event.
    // Often what happens, is all the ACTUAL work gets done when the screen is black (e.g. leaving
    // The server, and going back to the main menu) so that there isn't a really blunt cut
    ET_BEGIN_FADE = 0x11, // With data, can be in / out
    // In case some code has to be executed when the fade effect has finished
    ET_FADE_FINISHED = 0x12, // Just so that game can know when to do some sort of transition or something...

    /*
      Net = 0x2..
     */

    // When the user presses a join server button, needs to notify network code to send a join request to game server
    ET_REQUEST_TO_JOIN_SERVER = 0x20,
    // When game server replies, submit the ET_ENTER_SERVER event to notify for instance the world code to initialise the world
    ET_ENTER_SERVER = 0x21,
    // When a new player joins the game server, client will receive a packet saying a new player joined
    // The client program will submit this event to add the player to the list of players to interpolate
    // This is the same thing with the server program
    ET_NEW_PLAYER = 0x22,
    // Once the user id and the user tag of the current user is retrieved, this will be triggered,
    // Causing the client program to send an automatic login request to the meta server
    ET_START_CLIENT = 0x23,
    // Once the server is and server tag of the current server program is retrieved, this will be triggered
    // Causing the server program to send an automatic packet to the meta server notifying it of its online status
    ET_START_SERVER = 0x24,
    // Notifies the network module to tell the game server that we're getting out of here
    ET_LEAVE_SERVER = 0x25,
    // When another player leaves, this will notify the client (same thing with the server program)
    // To get rid of the player data in the world code
    ET_PLAYER_DISCONNECTED = 0x26,
    // When the client receives the response from the meta server, this event will notify the UI module
    // To update what is on the main menu page
    ET_RECEIVED_AVAILABLE_SERVERS = 0x27,
    ET_SEND_SERVER_TEAM_SELECT_REQUEST = 0x28,
    // When the username hasn't registered, request a username and password
    ET_REQUEST_USER_INFORMATION = 0x29,
    // When the user clicks on the sign up button to register a username and password, request gets sent
    // To the meta server - client waits for response
    ET_ATTEMPT_SIGN_UP = 0x2A,
    // Meta server has responded with the sign up success
    ET_SIGN_UP_SUCCESS = 0x2B,
    // Similar to above, except it's for logging in
    ET_ATTEMPT_LOGIN = 0x2C,
    ET_LOGIN_SUCCESS = 0x2D,
    ET_META_REQUEST_ERROR = 0x2E,
    // Conenction to a game server that is
    ET_CONNECTION_REQUEST_FAILED = 0x2F,

    /*
      Game = 0x3..
    */

    // This happens when the user presses the spawn button (with the "play" icon in the game menu)
    ET_SPAWN = 0x30,
    ET_LOCAL_PLAYER_DIED = 0x31,

    /*
      UIUX = 0x4..
    */

    // When user presses the refresh button on main menu (for available servers)
    // This event will notify the network module to send a request to the meta server requesting
    // All the available (online) game servers
    ET_REQUEST_REFRESH_SERVER_PAGE = 0x40,
    ET_EXIT_SCENE = 0x41,
    ET_ENTER_MAIN_MENU_SCENE = 0x42,
    // These two purely affect the main menu scene
    ET_ENTER_GAME_PLAY_SCENE = 0x43,
    ET_ENTER_MAP_CREATOR_SCENE = 0x44,
    ET_PAUSE = 0x45,
    ET_UNPAUSE = 0x46,
    // Event that is used to begin building maps
    ET_BEGIN_MAP_EDITING = 0x47,
    ET_CREATE_NEW_MAP = 0x48,
    ET_DONT_CREATE_NEW_MAP = 0x49,
    ET_MAP_EDITOR_CHOSE_COLOR = 0x4A,
    ET_PREPARE_GAMEPLAY_SCENE = 0x4B,
    ET_CLIENT_TOOK_DAMAGE = 0x4C,

    /*
      AI = 0x5..
    */

    // All AI is completely redundant at the moment.
    ET_BEGIN_AI_TRAINING = 0x50,
    ET_FINISH_GENERATION = 0x51,
    ET_RESET_AI_ARENA = 0x52,

    ET_INVALID_EVENT_TYPE

};

struct event_t {
    event_type_t type;

    /* 
      Likely to be allocated on free list allocator (for data types, see vkph_event_data.hpp).
    */
    void *data;
};

}
