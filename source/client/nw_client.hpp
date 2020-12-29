#pragma once

#include <stdint.h>
#include <vkph_state.hpp>
#include <net_game_server.hpp>

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

void nw_init(vkph::state_t *state);
void nw_tick(vkph::state_t *state);
bool nw_connected_to_server();
uint16_t nw_get_local_client_index();
void nw_check_registration(vkph::state_t *state);
net::available_servers_t *nw_get_available_servers();
