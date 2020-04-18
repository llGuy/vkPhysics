#include <common/log.hpp>
#include <common/tools.hpp>
#include <common/socket.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>

#define MAX_ACTIVE_SERVERS 50

static socket_t hub_socket;

static void s_hub_socket_init() {
    hub_socket = network_socket_init(SP_TCP);
    set_socket_to_non_blocking_mode(hub_socket);

    network_address_t address = {};
    address.port = host_to_network_byte_order(SERVER_HUB_OUTPUT_PORT);

    bind_network_socket_to_port(hub_socket, address);
    set_socket_to_listening(hub_socket, MAX_ACTIVE_SERVERS);
}

#define MAX_CONNECTED_CLIENTS 10000
#define MAX_ACTIVE_SERVERS 500
#define MAX_PENDING_SOCKETS 100

struct connection_t {
    socket_t sock;
    network_address_t address;
};

// Sockets for which we don't know if they are clients or servers
static uint32_t pending_count;
static connection_t *pending_sockets;

static stack_container_t<connection_t> client_sockets;
static stack_container_t<connection_t> server_sockets;

static void s_client_and_server_sockets_init() {
    client_sockets.init(MAX_CONNECTED_CLIENTS);
    server_sockets.init(MAX_ACTIVE_SERVERS);
    pending_count = 0;
    pending_sockets = FL_MALLOC(connection_t, MAX_PENDING_SOCKETS);

    uint32_t i = 0;
    for (; i < MAX_ACTIVE_SERVERS; ++i) {
        client_sockets[i] = {-1, {}};
        server_sockets[i] = {-1, {}};
    }

    for(; i < MAX_CONNECTED_CLIENTS; ++i) {
        client_sockets[i] = {-1, {}};
    }
}

static void s_check_new_connections() {
    accepted_connection_t new_connection = accept_connection(hub_socket);

    if (new_connection.s >= 0) {
        LOG_INFO("Received connection request, pushing to pending list\n");

        pending_sockets[pending_count++] = {new_connection.s, new_connection.address};
    }
    else {
        // Didn't actually receive any requests from any game servers
    }
}

static void s_check_new_queries() {
    
}

static void s_start_loop() {
    bool running = 1;

    while(running) {
        
    }
}

int32_t main(
    int32_t argc,
    char *argv[]) {
    socket_api_init();
    s_hub_socket_init();
    s_start_loop();
}
