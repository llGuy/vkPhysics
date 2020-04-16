#include "log.hpp"
#include "containers.hpp"

#include "socket.hpp"

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

static void s_set_socket_recv_buffer_size(
    socket_t s,
    uint32_t size) {
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&size, sizeof(size)) == -1) {
        LOG_ERROR("Failed to set socket buffer size\n");
    }
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

static void s_set_socket_to_listening(
    socket_t s,
    uint32_t max_clients) {
    SOCKET *api_s = get_network_socket(s);
    if (listen(*api_s, max_clients) < 0) {
        LOG_ERROR("Failed to set socket to listening mode\n");
        exit(1);
    }
}

static void s_set_socket_to_non_blocking_mode(
    socket_t s) {
    SOCKET *sock = get_network_socket(s);
    u_long enabled = 1;
    ioctlsocket(*sock, FIONBIO, &enabled);
}

static accepted_connection_t s_accept_connection(
    socket_t s) {
    SOCKET *sock = get_network_socket(s);
    SOCKADDR_IN from_address = {};
    int32_t from_size = sizeof(from_address);

    SOCKET new_socket = accept(*sock, (SOCKADDR *)&from_address, &from_size);

    socket_t new_s = sockets.add();

    SOCKET *api_s = get_network_socket(new_s);
    *api_s = new_socket;

    accepted_connection_t connection = {};
    connection.s = new_s;
    connection.address = { from_address.sin_port, from_address.sin_addr.S_un.S_addr };

    return connection;
}
static void s_connect_to_address(
    socket_t s,
    network_address_t address) {
    SOCKET *sock = get_network_socket(s);
    
    SOCKADDR_IN saddr = {};
    saddr.sin_family = AF_INET;
    saddr.sin_port = address.port;
    saddr.sin_addr.s_addr = address.ipv4_address;
    int32_t addr_length = sizeof(saddr);

    if (connect(s, (SOCKADDR *)&saddr, addr_length) < 0) {
        LOG_ERROR("Failed to connect to address with connect()\n");
    }
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

static bool s_send_to_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    SOCKET *sock = get_network_socket(s);
    
    int32_t send_ret = send(*sock, buffer, buffer_size, 0);

    if (send_ret == SOCKET_ERROR) {
        char error_n[32];
        sprintf_s(error_n, "sendto failed: %d\n", WSAGetLastError());
        //OutputDebugString(error_n);
        LOG_ERROR(error_n);
        assert(0);
    }

    return(send_ret != SOCKET_ERROR);
}

static int32_t s_receive_from_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    SOCKET *sock = get_network_socket(s);

    int32_t bytes_received = recv(*sock, buffer, buffer_size, 0);

    if (bytes_received == SOCKET_ERROR) {
        return 0;
    }
    else {
        buffer[bytes_received] = 0;
    }

    return(bytes_received);
}

#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
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

static void s_set_socket_recv_buffer_size(
    socket_t s,
    uint32_t size) {
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
        LOG_ERROR("Failed to set socket buffer size\n");
    }
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

static accepted_connection_t s_accept_connection(
    socket_t s) {
    sockaddr_in from_address = {};
    socklen_t from_size = sizeof(from_address);

    socket_t new_socket = accept(s, (sockaddr *)&from_address, &from_size);

    accepted_connection_t connection = {};
    connection.s = new_socket;
    connection.address = { from_address.sin_port, from_address.sin_addr.s_addr };

    return connection;
}

static void s_set_socket_to_listening(
    socket_t s,
    uint32_t max_clients) {
    if (listen(s, max_clients) < 0) {
        LOG_ERROR("Failed to set socket to listening mode\n");
        exit(1);
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

static int32_t s_receive_from_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    int32_t bytes_received = recv(s, buffer, buffer_size, 0);

    if (bytes_received < 0) {
        return 0;
    }
    else {
        buffer[bytes_received] = 0;
    }

    return bytes_received;
}

static bool s_send_to_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    int32_t send_ret = send(s, buffer, buffer_size, 0);

    if (send_ret < 0) {
        // Error
        LOG_ERRORV("sendto: %s\n", strerror(errno));

        return 0;
    }
    else {
        return 1;
    }
}

static void s_connect_to_address(
    socket_t s,
    network_address_t address) {
    sockaddr_in saddr = {};
    saddr.sin_family = AF_INET;
    saddr.sin_port = address.port;
    saddr.sin_addr.s_addr = address.ipv4_address;
    socklen_t addr_length = sizeof(saddr);

    if (connect(s, (sockaddr *)&saddr, addr_length) < 0) {
        LOG_ERROR("Failed to connect to address with connect()\n");
    }
}

static uint32_t s_str_to_ipv4_int32(
    const char *name,
    uint32_t port,
    int32_t protocol) {
    addrinfo hints = {}, *addresses;

    hints.ai_family = AF_INET;

    switch (protocol) {
    case SP_UDP: {
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        break;
    }
        
    case SP_TCP: {
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        break;
    }
    }

    char port_str[16] = {};
    sprintf(port_str, "%d", port);

    int32_t err = getaddrinfo(name, port_str, &hints, &addresses);

    if (err != 0) {
        fprintf(stderr, "%s: %s\n", name, gai_strerror(err));
        abort();
    }

    // (This is a really bad way)
    // Find way to get correct address
    if (addresses) {
        for (addrinfo *addr = addresses; addr != NULL; addr = addr->ai_next) {
            if (addr->ai_family == AF_INET) {
                sockaddr_in *addr_in = (sockaddr_in *)addr->ai_addr;
                return addr_in->sin_addr.s_addr;
            }
        }

        freeaddrinfo(addresses);

        LOG_INFOV("Couldn't find address %s\n", name);
        return 0;
    }
    else {
        return 0;
    }
}

#endif

void socket_api_init() {
    s_api_init();
}

socket_t network_socket_init(
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

void bind_network_socket_to_port(
    socket_t s,
    network_address_t address) {
    s_bind_network_socket_to_port(s, address);
}

void set_socket_to_listening(
    socket_t s,
    uint32_t max_clients) {
    s_set_socket_to_listening(
        s,
        max_clients);
}

void set_socket_to_non_blocking_mode(
    socket_t s) {
    s_set_socket_to_non_blocking_mode(s);
}

int32_t receive_from(
    socket_t s,
    char *buffer,
    uint32_t buffer_size,
    network_address_t *address_dst) {
    return s_receive_from(s, buffer, buffer_size, address_dst);
}

bool send_to(
    socket_t s,
    network_address_t address,
    char *buffer,
    uint32_t buffer_size) {
    return s_send_to(s, address, buffer, buffer_size);
}

uint32_t str_to_ipv4_int32(
    const char *address,
    uint32_t port,
    int32_t protocol) {
    return s_str_to_ipv4_int32(address, port, protocol);
    //return inet_addr(address);
}

uint16_t host_to_network_byte_order(
    uint16_t bytes) {
    return htons(bytes);
}

void set_socket_recv_buffer_size(
    socket_t s,
    uint32_t size) {
    s_set_socket_recv_buffer_size(s, size);
}

accepted_connection_t accept_connection(
    socket_t s) {
    return s_accept_connection(s);
}

void connect_to_address(
    socket_t s,
    network_address_t address) {
    s_connect_to_address(s, address);
}

int32_t receive_from_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    return s_receive_from_bound_address(
        s,
        buffer,
        buffer_size);
}

bool send_to_bound_address(
    socket_t s,
    char *buffer,
    uint32_t buffer_size) {
    return s_send_to_bound_address(
        s,
        buffer,
        buffer_size);
}
