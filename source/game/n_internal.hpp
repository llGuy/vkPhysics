#pragma once

#include <common/tools.hpp>

void n_socket_api_init();

struct network_address_t {
    uint16_t port;
    uint32_t ipv4_address;
};

typedef int32_t socket_t;

enum socket_protocol_t { SP_TCP, SP_UDP };

socket_t n_network_socket_init(
    socket_protocol_t protocol);

void n_bind_network_socket_to_port(
    socket_t s,
    network_address_t address);

void n_set_socket_to_non_blocking_mode(
    socket_t s);

int32_t n_receive_from(
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst);

bool n_send_to(
    network_address_t address,
    char *buffer,
    uint32_t buffer_size);

uint32_t n_str_to_ipv4_int32(
    const char *address);

uint32_t n_host_to_network_byte_order(
    uint32_t bytes);

uint32_t n_network_to_host_byte_order(
    uint32_t bytes);

float n_host_to_network_byte_order_f32(
    float bytes);

float n_network_to_host_byte_order_f32(
    float bytes);
