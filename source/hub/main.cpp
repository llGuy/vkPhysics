#include <common/log.hpp>
#include <common/tools.hpp>
#include <common/socket.hpp>

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

static void s_start_loop() {
    bool running = 1;

    while(running) {
        accepted_connection_t new_connection = accept_connection(hub_socket);

        if (new_connection.s >= 0) {
            LOG_INFO("Received connection request\n");
        }
        else {
            // Didn't actually receive any requests from any game servers
        }
    }
}

static void foo(
    int32_t something) {
     
}

int32_t main(
    int32_t argc,
    char *argv[]) {
    socket_api_init();
    s_hub_socket_init();
    s_start_loop();
}
