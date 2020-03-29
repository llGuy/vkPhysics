#include "n_internal.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define _WINSOCKAPI_

static void s_api_init() {
    WSADATA winsock_data;
    if (WSAStartup(0x202, &winsock_data)) {
        printf("Failed to initialise Winsock\n");
        assert(0);
    }
}

#else
#include <arpa/inet.h>
#include <sys/socket.h>

static void s_api_init() {
    // Doesn't do anything
}

#endif

void n_socket_api_init() {
    s_api_init();
}

socket_t n_add_network_socket() {
    
}

void n_network_socket_init(
    socket_t s,
    int32_t family,
    int32_t type,
    int32_t protocol) {
    
}

void n_bind_network_socket_to_port(
    socket_t s,
    network_address_t address) {
    
}

void n_set_socket_to_non_blocking_mode(
    socket_t *s) {
    
}

int32_t n_receive_from(
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst) {
    
}

bool n_send_to(
    network_address_t address,
    char *buffer,
    uint32_t buffer_size) {
    
}

uint32_t n_str_to_ipv4_int32(
    const char *address) {
    
}

uint32_t n_host_to_network_byte_order(
    uint32_t bytes) {
    
}

uint32_t n_network_to_host_byte_order(
    uint32_t bytes) {
    
}

float n_host_to_network_byte_order_f32(
    float bytes) {
    
}

float n_network_to_host_byte_order_f32(
    float bytes) {
    
}
