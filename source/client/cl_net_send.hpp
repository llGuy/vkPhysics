#pragma once

#include <vkph_state.hpp>
#include <net_context.hpp>

namespace cl {

void send_packet_client_commands(
    vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server,
    bool simulate_lag);

bool send_packet_connection_request(
    uint32_t ip_address,
    struct local_client_info_t *info,
    net::context_t *ctx,
    net::game_server_t *server);

void send_packet_team_select_request(
    vkph::team_color_t color,
    const vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server);

void send_packet_client_disconnect(
    const vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server);

}
