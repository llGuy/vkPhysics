#pragma once

#include "net_socket.hpp"
#include <vkph_state.hpp>
#include <net_context.hpp>

namespace cl {

void prepare_receiving();

/*
  These functions also handle the information that the packets hold.
 */

bool receive_packet_connection_handshake(
    serialiser_t *serialiser,
    uint32_t server_tag,
    vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server);

void receive_packet_player_joined(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx);

void receive_packet_player_left(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx);

void receive_packet_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    vkph::state_t *state,
    net::context_t *ctx);

void receive_packet_chunk_voxels(
    serialiser_t *serialiser,
    vkph::state_t *state);

void receive_player_team_change(
    serialiser_t *serialiser,
    vkph::state_t *state);

void receive_ping(
    serialiser_t *in_serialiser,
    vkph::state_t *state,
    net::context_t *ctx,
    net::game_server_t *server_addr);

void check_if_finished_recv_chunks(vkph::state_t *state, net::context_t *ctx);

}
