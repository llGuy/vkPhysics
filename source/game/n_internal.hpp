#pragma once

#include "net.hpp"
#include <common/tools.hpp>
#include <common/serialiser.hpp>

void n_socket_api_init();

typedef int32_t socket_t;

enum socket_protocol_t { SP_TCP, SP_UDP };

void n_socket_api_init();

socket_t n_network_socket_init(
    socket_protocol_t protocol);

void n_bind_network_socket_to_port(
    socket_t s,
    network_address_t address);

void n_set_socket_to_non_blocking_mode(
    socket_t s);

int32_t n_receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst);

bool n_send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size);

uint32_t n_str_to_ipv4_int32(
    const char *address);

uint16_t n_host_to_network_byte_order(
    uint16_t bytes);

uint16_t n_network_to_host_byte_order(
    uint16_t bytes);

float n_host_to_network_byte_order_f32(
    float bytes);

float n_network_to_host_byte_order_f32(
    float bytes);

enum packet_type_t { PT_CONNECTION_REQUEST, PT_CONNECTION_HANDSHAKE };

struct packet_header_t {
    union {
        struct {
            uint32_t packet_type: 5;
            // Includes header
            uint32_t total_packet_size: 26;
        };

        uint32_t bytes;
    } flags;

    uint64_t current_tick;
    uint64_t current_packet_count;
    uint32_t client_id;
};

uint32_t n_packed_packet_header_size();

void n_serialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser);

void n_deserialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser);

#define CLIENT_NAME_MAX_LENGTH 50

struct packet_connection_request_t {
    const char *name;
};

uint32_t n_packed_connection_request_size(
    packet_connection_request_t *connection_request);

void n_deserialise_connection_request(
    packet_connection_request_t *request,
    serialiser_t *serialiser);

void n_serialise_connection_request(
    packet_connection_request_t *packet,
    serialiser_t *serialiser);

struct full_player_info_t {
    const char *name;
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;
    bool is_local;
};

// Will use this when new player joins
struct packet_connection_handshake_t {
    uint32_t player_count;
    full_player_info_t *player_infos;
    // Chunks will be sent in separate packets (too much data)
};

uint32_t n_packed_connection_handshake_size(
    packet_connection_handshake_t *game_state);

void n_serialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser);

void n_deserialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser);

// Will use this during game play
struct game_state_snapshot_t {
    
};
