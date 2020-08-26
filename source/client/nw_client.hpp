#pragma once

#include <stdint.h>

struct local_client_info_t {
    const char *name;
};

// #define DEBUG_NETWORKING

#if defined (DEBUG_NETWORKING)
#define LOG_NETWORK_DEBUG(text) LOG_INFO(text)
#define LOG_NETWORK_DEBUGV(text, ...) LOG_INFOV(text, __VA_ARGS__)
#else
#define LOG_NETWORK_DEBUG(text)
#define LOG_NETWORK_DEBUGV(text, ...)
#endif

void nw_init(struct event_submissions_t *events);
void nw_tick(struct event_submissions_t *events);
bool nw_connected_to_server();
uint16_t nw_get_local_client_index();
void nw_check_registration(event_submissions_t *events);
