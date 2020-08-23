#pragma once

#include <stdint.h>

enum request_t {
    // Requests to create an account
    R_SIGN_UP,
    // Requests to login to an existing account
    R_LOGIN,
    // Requests for the available servers (which are active)
    R_AVAILABLE_SERVERS,
    // Whenever a client begins the game, this request gets sent to log the player in
    R_AUTOMATIC_LOGIN,

    // Registers server (tells the meta server the name)
    R_REGISTER_SERVER,
    // Tells the meta server that the running game server just started being online
    R_SERVER_ACTIVE,
    // Tells the meta server that the running game server just stopped being online
    R_SERVER_INACTIVE,

    // Just makes sure that the request thread stops
    R_QUIT,
    R_INVALID
};

enum request_error_t {
    RE_USERNAME_EXISTS,
    RE_INCORRECT_PASSWORD_OR_USERNAME,
    RE_INVALID
};

struct request_sign_up_data_t {
    const char *username;
    const char *password;
};

struct request_login_data_t {
    const char *username;
    const char *password;
};

struct request_automatic_login_t {
    uint32_t usertag;
    uint32_t userid;
};

struct request_available_server_t {
    // Doesn't contain anything
};

struct request_register_server_t {
    const char *server_name;
};

struct request_server_active_t {
    uint32_t server_id;
};

struct request_server_inactive_t {
    uint32_t server_id;
};

// TODO: Free memory that is used to create the request in the main thread

// Starts the thread from which HTTP requests to the meta server
// will be made
void begin_meta_client_thread();
char *check_request_finished(uint32_t *size, request_t *request);
void send_request(request_t request, void *request_data);
void join_meta_thread();
