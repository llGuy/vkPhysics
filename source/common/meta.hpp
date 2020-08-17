#pragma once

#include <stdint.h>

enum request_t {
    R_SIGN_UP,
    R_LOGIN,
    R_AVAILABLE_SERVERS,
    R_QUIT,
    // TODO:
    R_INVALID
};

enum request_error_t {
    RE_USERNAME_EXISTS,
    RE_INCORRECT_PASSWORD,
    RE_INVALID
};

struct request_sign_up_data_t {
    const char *username;
    const char *password;
};

struct request_login_data_t {
    uint32_t usertag;
    uint32_t userid;
};

struct request_available_server_t {
    // Doesn't contain anything
};

// TODO: Free memory that is used to create the request in the main thread

// Starts the thread from which HTTP requests to the meta server
// will be made
void begin_meta_client_thread();
char *check_request_finished(uint32_t *size, request_t *request);
void send_request(request_t request, void *request_data);
void join_meta_thread();
