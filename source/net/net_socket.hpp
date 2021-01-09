#pragma once

#include <stdint.h>

namespace net {

using socket_t = int32_t;

struct address_t {
    uint16_t port;
    uint32_t ipv4_address;
};

struct accepted_connection_t {
    socket_t s;
    address_t address;
    bool success;
};

void socket_api_init();

enum socket_protocol_t { SP_TCP, SP_UDP };

void init_socket_api();
socket_t network_socket_init(socket_protocol_t protocol);
void destroy_socket(socket_t s);
void set_socket_recv_buffer_size(socket_t s, uint32_t size);
void set_socket_send_buffer_size(socket_t s, uint32_t size);
uint16_t bind_network_socket_to_port(socket_t s, address_t address);
void set_socket_to_listening(socket_t s, uint32_t max_clients);
// If enabled = 1, socket will be blocking
// If enabled = 0, socket will be non blocking
void set_socket_blocking_state(socket_t s, bool enabled);
accepted_connection_t accept_connection(socket_t s);
bool connect_to_address(socket_t s, const char *address_name, uint16_t port, int32_t protocol);
int32_t receive_from(socket_t s, char *buffer, uint32_t buffer_size, address_t *address_dst);
bool send_to(socket_t s, address_t address, char *buffer, uint32_t buffer_size);
int32_t receive_from_bound_address(socket_t s, char *buffer, uint32_t buffer_size);
bool send_to_bound_address(socket_t s, char *buffer, uint32_t buffer_size);
uint32_t str_to_ipv4_int32(const char *address, uint32_t port, int32_t protocol);
uint16_t host_to_network_byte_order(uint16_t bytes);
float host_to_network_byte_order_f32(float bytes);
float network_to_host_byte_order_f32(float bytes);

}
