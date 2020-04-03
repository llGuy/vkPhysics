#include "net.hpp"
#include "n_internal.hpp"
#include <common/event.hpp>
#include <common/string.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>

#define MAX_MESSAGE_SIZE 40000

static char *message_buffer;

static listener_t net_listener_id;

static void s_net_event_listener(
    void *object,
    event_t *event) {
    switch (event->type) {

    case ET_START_CLIENT: {
        start_client();
    } break;

    case ET_START_SERVER: {
        start_server();
    } break;

    }
}

void net_init(
    event_submissions_t *events) {
    net_listener_id = set_listener_callback(
        &s_net_event_listener,
        NULL,
        events);

    n_socket_api_init();

    message_buffer = FL_MALLOC(char, MAX_MESSAGE_SIZE);
}

static socket_t main_udp_socket;

#define GAME_OUTPUT_PORT_CLIENT 6001
#define GAME_OUTPUT_PORT_SERVER 6000

static void s_main_udp_socket_init(
    uint16_t output_port) {
    main_udp_socket = n_network_socket_init(SP_UDP);
    network_address_t address = {};
    address.port = n_host_to_network_byte_order(output_port);
    n_bind_network_socket_to_port(main_udp_socket, address);
    n_set_socket_to_non_blocking_mode(main_udp_socket);
}

static bool started_client = 0;

void start_client() {
    s_main_udp_socket_init(GAME_OUTPUT_PORT_CLIENT);

    started_client = 1;
}

static network_address_t bound_server_address;

static void s_process_connection_handshake(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_connection_handshake_t handshake = {};
    n_deserialise_connection_handshake(&handshake, serialiser);

    event_enter_server_t *data = FL_MALLOC(event_enter_server_t, 1);
    data->local_client_id = handshake.client_id;
    submit_event(ET_ENTER_SERVER, data, events);
}

void tick_client(
    event_submissions_t *events) {
    network_address_t received_address = {};
    int32_t received = n_receive_from(
        main_udp_socket,
        message_buffer,
        sizeof(char) * MAX_MESSAGE_SIZE,
        &received_address);

    // In future, separate thread will be capturing all these packets
    static const uint32_t MAX_RECEIVED_PER_TICK = 3;
    uint32_t i = 0;

    while (received && i < MAX_RECEIVED_PER_TICK) {
        serialiser_t in_serialiser = {};
        in_serialiser.data_buffer = (uint8_t *)message_buffer;
        in_serialiser.data_buffer_size = received;

        packet_header_t header = {};
        n_deserialise_packet_header(&header, &in_serialiser);

        switch(header.flags.packet_type) {

        case PT_CONNECTION_HANDSHAKE: {
            s_process_connection_handshake(
                &in_serialiser,
                events);
        } break;

        }
        
        received = n_receive_from(
            main_udp_socket,
            message_buffer,
            sizeof(char) * MAX_MESSAGE_SIZE,
            &received_address);

        ++i;
    }
}

void send_connect_request_to_server(
    const char *ip_address,
    local_client_info_t *info) {
    bound_server_address.port = n_host_to_network_byte_order(GAME_OUTPUT_PORT_SERVER);
    bound_server_address.ipv4_address = n_str_to_ipv4_int32(ip_address);

    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_connection_request_t request = {};
    request.name = info->name;
    
    packet_header_t header = {};
    header.flags.packet_type = PT_CONNECTION_REQUEST;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_connection_request_size(&request);

    n_serialise_packet_header(&header, &serialiser);
    n_serialise_connection_request(&request, &serialiser);

    n_send_to(main_udp_socket, bound_server_address, (char *)serialiser.data_buffer, serialiser.data_buffer_head);
}

#define MAX_CLIENTS 50

static stack_container_t<client_t> clients;

static bool started_server = 0;

void start_server() {
    s_main_udp_socket_init(GAME_OUTPUT_PORT_SERVER);

    clients.init(MAX_CLIENTS);

    started_server = 1;
}

static void s_process_connection_request(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_connection_request_t request = {};
    n_deserialise_connection_request(&request, serialiser);

    uint32_t client_id = clients.add();
    client_t *client = clients.get(client_id);
    
    client->client_id = client_id;
    client->name = create_fl_string(request.name);

    event_new_player_t *event_data = FL_MALLOC(event_new_player_t, 1);
    event_data->client_data = client;
    
    submit_event(ET_NEW_PLAYER, event_data, events);
}

void tick_server(
    event_submissions_t *events) {
    // In future, have a separate thread capturing these
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        network_address_t received_address = {};
        int32_t received = n_receive_from(
            main_udp_socket,
            message_buffer,
            sizeof(char) * MAX_MESSAGE_SIZE,
            &received_address);

        if (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)message_buffer;
            in_serialiser.data_buffer_size = received;

            packet_header_t header = {};
            n_deserialise_packet_header(&header, &in_serialiser);

            switch(header.flags.packet_type) {

            case PT_CONNECTION_REQUEST: {
                s_process_connection_request(
                    &in_serialiser,
                    events);
            } break;

            }
        }
    }
}

void tick_net(
    event_submissions_t *events) {
    if (started_client) {
        tick_client(events);
    }
    if (started_server) {
        tick_server(events);
    }
}
