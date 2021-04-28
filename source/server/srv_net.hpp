#pragma once

#include <vkph_state.hpp>
#include <net_game_client.hpp>

// #define DEBUG_NETWORKING

#if defined (DEBUG_NETWORKING)
#define LOG_NETWORK_DEBUG(text) LOG_INFO(text)
#define LOG_NETWORK_DEBUGV(text, ...) LOG_INFOV(text, __VA_ARGS__)
#else
#define LOG_NETWORK_DEBUG(text)
#define LOG_NETWORK_DEBUGV(text, ...)
#endif

namespace srv {

void init_net(vkph::state_t *state);
void tick_net(vkph::state_t *state);
const net::client_t *get_client(uint32_t i);

}
