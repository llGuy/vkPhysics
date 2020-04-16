#pragma once

#include "tools.hpp"

struct network_address_t {
    uint16_t port;
    uint32_t ipv4_address;
};

void socket_api_init();

typedef int32_t socket_t;

enum socket_protocol_t { SP_TCP, SP_UDP };

void socket_api_init();

socket_t network_socket_init(
    socket_protocol_t protocol);

void set_socket_recv_buffer_size(
    socket_t s,
    uint32_t size);

void bind_network_socket_to_port(
    socket_t s,
    network_address_t address);

void set_socket_to_listening(
    socket_t s,
    uint32_t max_clients);

void set_socket_to_non_blocking_mode(
    socket_t s);

struct accepted_connection_t {
    socket_t s;
    network_address_t address;
};

accepted_connection_t accept_connection(
    socket_t s);

void connect_to_address(
    socket_t s,
    network_address_t address);

int32_t receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst);

bool send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size);

int32_t receive_from_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size);

bool send_to_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size);

uint32_t str_to_ipv4_int32(
    const char *address,
    uint32_t port,
    int32_t protocol);

uint16_t host_to_network_byte_order(
    uint16_t bytes);

float host_to_network_byte_order_f32(
    float bytes);

float network_to_host_byte_order_f32(
    float bytes);

// Port that hub uses to commmunicate with servers / clients
#define SERVER_HUB_OUTPUT_PORT 5998
// Port that clients / servers uses to communicate with hub
#define GAME_OUTPUT_HUB_PORT 5999
#define GAME_OUTPUT_PORT_CLIENT 6001
#define GAME_OUTPUT_PORT_SERVER 6000
