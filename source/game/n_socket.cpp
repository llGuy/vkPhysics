#include "n_internal.hpp"
#include <common/log.hpp>
#include <common/containers.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define _WINSOCKAPI_

#pragma comment(lib, "Ws2_32.lib")

static stack_container_t<SOCKET> sockets;

static void s_api_init() {
    sockets.init(50);

    WSADATA winsock_data;
    if (WSAStartup(0x202, &winsock_data)) {
        LOG_ERROR("Failed to initialise Winsock\n");
        assert(0);
    }
}

static SOCKET *get_network_socket(socket_t socket) {
    return(sockets.get(socket));
}

static socket_t s_network_socket_init(
    int32_t family,
    int32_t type,
    int32_t protocol) {
    socket_t s = sockets.add();

    SOCKET *api_s = get_network_socket(s);
    *api_s = socket(family, type, protocol);

    return s;
}

static void s_bind_network_socket_to_port(
    socket_t s,
    network_address_t address) {
    SOCKET *api_s = get_network_socket(s);

    sockaddr_in address_struct = {};
    address_struct.sin_family = AF_INET;
    address_struct.sin_port = address.port;
    address_struct.sin_addr.s_addr = INADDR_ANY;

    if (bind(*api_s, (sockaddr *)&address_struct, sizeof(address_struct)) < 0) {
        LOG_WARNING("Failed to bind socket to port, trying another\n");

        ++address.port;
        s_bind_network_socket_to_port(s, address);
    }
    else {
        LOG_INFO("Sucess bind socket to port\n");
    }
}

static void s_set_socket_to_non_blocking_mode(
    socket_t s) {
    SOCKET *sock = get_network_socket(s);
    u_long enabled = 1;
    ioctlsocket(*sock, FIONBIO, &enabled);
}

static int32_t s_receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst) {
    SOCKET *sock = get_network_socket(s);

    SOCKADDR_IN from_address = {};
    int32_t from_size = sizeof(from_address);
    
    int32_t bytes_received = recvfrom(*sock, buffer, buffer_size, 0, (SOCKADDR *)&from_address, &from_size);

    if (bytes_received == SOCKET_ERROR) {
        return 0;
    }
    else {
        buffer[bytes_received] = 0;
    }

    network_address_t received_address = {};
    received_address.port = from_address.sin_port;
    received_address.ipv4_address = from_address.sin_addr.S_un.S_addr;

    *address_dst = received_address;
    
    return(bytes_received);
}

static bool s_send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size) {
    SOCKET *sock = get_network_socket(s);
    
    SOCKADDR_IN address_struct = {};
    address_struct.sin_family = AF_INET;
    // Needs to be in network byte order
    address_struct.sin_port = address.port;
    address_struct.sin_addr.S_un.S_addr = address.ipv4_address;
    
    int32_t address_size = sizeof(address_struct);

    int32_t sendto_ret = sendto(*sock, buffer, buffer_size, 0, (SOCKADDR *)&address_struct, sizeof(address_struct));

    if (sendto_ret == SOCKET_ERROR) {
        char error_n[32];
        sprintf_s(error_n, "sendto failed: %d\n", WSAGetLastError());
        //OutputDebugString(error_n);
        LOG_ERROR(error_n);
        assert(0);
    }

    return(sendto_ret != SOCKET_ERROR);
}

#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

static void s_api_init() {
    // Doesn't do anything
}

static socket_t s_network_socket_init(
    int32_t family,
    int32_t type,
    int32_t protocol) {
    return socket(family, type, protocol);
}

static void s_bind_network_socket_to_port(
    socket_t s,
    network_address_t address) {
    sockaddr_in address_struct = {};
    address_struct.sin_family = AF_INET;
    address_struct.sin_port = address.port;
    address_struct.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (sockaddr *)&address_struct, sizeof(address_struct)) < 0) {
        LOG_WARNING("Failed to bind socket to port, trying another\n");

        ++address.port;
        s_bind_network_socket_to_port(s, address);
    }
    else {
        LOG_INFO("Success bind socket to port\n");
    }
}

static void s_set_socket_to_non_blocking_mode(
    socket_t s) {
    int32_t flags = fcntl(s, F_GETFL, 0);
    
    if (flags == -1) {
        // Error
    }

    flags = flags | O_NONBLOCK;
    fcntl(s, F_SETFL, flags);
}

static int32_t s_receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst) {
    sockaddr_in from_address = {};
    socklen_t from_size = sizeof(from_address);

    int32_t bytes_received = recvfrom(s, buffer, buffer_size, 0, (sockaddr *)&from_address, &from_size);

    if (bytes_received < 0) {
        return 0;
    }
    else {
        buffer[bytes_received] = 0;
    }

    network_address_t received_address = {};
    received_address.port = from_address.sin_port;
    received_address.ipv4_address = from_address.sin_addr.s_addr;

    *address_dst = received_address;

    return bytes_received;
}

static bool s_send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size) {
    sockaddr_in address_struct = {};
    address_struct.sin_family = AF_INET;
    address_struct.sin_port = address.port;
    address_struct.sin_addr.s_addr = address.ipv4_address;

    int32_t sendto_ret = sendto(s, buffer, buffer_size, 0, (sockaddr *)&address_struct, sizeof(address_struct));

    if (sendto_ret < 0) {
        // Error
        LOG_ERRORV("sendto: %s\n", strerror(errno));

        return 0;
    }
    else {
        return 1;
    }
}

#endif

void n_socket_api_init() {
    s_api_init();
}

socket_t n_network_socket_init(
    socket_protocol_t protocol) {
    // Only support IPv4
    int32_t family = AF_INET;
    int32_t type;
    int32_t proto;

    switch (protocol) {
    case SP_UDP: {
        type = SOCK_DGRAM;
        proto = IPPROTO_UDP;
        break;
    }
        
    case SP_TCP: {
        type = SOCK_STREAM;
        proto = IPPROTO_TCP;
        break;
    }
    }
    
    return s_network_socket_init(family, type, proto);
}

void n_bind_network_socket_to_port(
    socket_t s,
    network_address_t address) {
    s_bind_network_socket_to_port(s, address);
}

void n_set_socket_to_non_blocking_mode(
    socket_t s) {
    s_set_socket_to_non_blocking_mode(s);
}

int32_t n_receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst) {
    return s_receive_from(s, buffer, buffer_size, address_dst);
}

bool n_send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size) {
    return s_send_to(s, address, buffer, buffer_size);
}

uint32_t n_str_to_ipv4_int32(
    const char *address) {
    // Use inet_pton maybe?
    return inet_addr(address);
}

uint16_t n_host_to_network_byte_order(
    uint16_t bytes) {
    return htons(bytes);
}

/*uint32_t n_network_to_host_byte_order(
    uint32_t bytes) {
    return ntohl(bytes);
}*/
