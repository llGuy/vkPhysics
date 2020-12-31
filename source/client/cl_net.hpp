#pragma once

#include <stdint.h>
#include <vkph_state.hpp>
#include <net_game_server.hpp>

namespace cl {

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

void init_net(vkph::state_t *state);
void tick_net(vkph::state_t *state);
bool is_connected_to_server();
uint16_t &get_local_client_index();
void check_registration(vkph::state_t *state);

}
