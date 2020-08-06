#include "net.hpp"
#include "string.hpp"
#include "socket.hpp"
#include "hub_packet.hpp"

net_data_t g_net_data = {};

static socket_t hub_socket;
static socket_t main_udp_socket;

void main_udp_socket_init(uint16_t output_port) {
    g_net_data.current_packet = 0;

    main_udp_socket = network_socket_init(SP_UDP);
    network_address_t address = {};
    address.port = host_to_network_byte_order(output_port);
    bind_network_socket_to_port(main_udp_socket, address);
    set_socket_to_non_blocking_mode(main_udp_socket);
    set_socket_recv_buffer_size(main_udp_socket, 1024 * 1024);
}

#define HUB_SERVER_DOMAIN "www.llguy.fun"

void hub_socket_init() {
    hub_socket = network_socket_init(SP_TCP);
    connect_to_address(hub_socket, HUB_SERVER_DOMAIN, SERVER_HUB_OUTPUT_PORT, SP_TCP);
    set_socket_to_non_blocking_mode(hub_socket);
}

bool send_to_game_server(
    serialiser_t *serialiser,
    network_address_t address) {
    ++g_net_data.current_packet;
    return send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

bool send_to_hub_server(
    serialiser_t *serialiser) {
    return send_to_bound_address(hub_socket, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

void acc_predicted_modification_init(accumulated_predicted_modification_t *apm_ptr, uint64_t tick) {
    if (!apm_ptr->acc_predicted_modifications) {
        apm_ptr->acc_predicted_modifications = (chunk_modifications_t *)g_net_data.chunk_modification_allocator.allocate_arena();
    }

    apm_ptr->acc_predicted_chunk_mod_count = 0;
    apm_ptr->tick = tick;
    memset(
        apm_ptr->acc_predicted_modifications,
        0,
        sizeof(chunk_modifications_t) * NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);
}

accumulated_predicted_modification_t *add_acc_predicted_modification() {
    return g_net_data.acc_predicted_modifications.push_item();
}

// HPT_RESPONSE_AVAILABLE_SERVERS
static void s_receive_response_available_servers(
    serialiser_t *serialiser,
    event_submissions_t *submission) {
    g_net_data.available_servers.name_to_server.clear();
    
    hub_response_available_servers_t response = {};
    deserialise_hub_response_available_servers(&response, serialiser);

    LOG_INFOV("There are %i available servers\n", response.server_count);

    g_net_data.available_servers.server_count = response.server_count;
    for (uint32_t i = 0; i < response.server_count; ++i) {
        hub_server_info_t *src = &response.servers[i];
        game_server_t *dst = &g_net_data.available_servers.servers[i];

        if (dst->server_name) {
            FL_FREE((void *)dst->server_name);
        }
        
        dst->ipv4_address = src->ipv4_address;
        dst->server_name = create_fl_string(src->server_name);
        
        g_net_data.available_servers.name_to_server.insert(simple_string_hash(dst->server_name), i);
    }

    submit_event(ET_RECEIVED_AVAILABLE_SERVERS, NULL, submission);
}

void check_incoming_hub_server_packets(event_submissions_t *events) {
    int32_t received = receive_from_bound_address(
        hub_socket,
        g_net_data.message_buffer,
        sizeof(char) * NET_MAX_MESSAGE_SIZE);

    // In future, separate thread will be capturing all these packets
    static const uint32_t MAX_RECEIVED_PER_TICK = 4;
    uint32_t i = 0;

    while (received) {
        serialiser_t in_serialiser = {};
        in_serialiser.data_buffer = (uint8_t *)g_net_data.message_buffer;
        in_serialiser.data_buffer_size = received;

        hub_packet_header_t header = {};
        deserialise_hub_packet_header(&header, &in_serialiser);

        switch (header.type) {
        case HPT_RESPONSE_AVAILABLE_SERVERS: {
            s_receive_response_available_servers(&in_serialiser, events);
            break;
        }
        case HPT_QUERY_RESPONSIVENESS: {
            // HPT_QUERY_RESPONSIVENESS
            hub_packet_header_t new_header = {};
            new_header.type = HPT_RESPONSE_RESPONSIVENESS;

            serialiser_t serialiser = {};
            serialiser.init(20);

            serialise_hub_packet_header(&new_header, &serialiser);

            send_to_bound_address(hub_socket, (char *)serialiser.data_buffer, serialiser.data_buffer_head);
            break;
        }
        default: {
            break;
        }
        }

        if (i < MAX_RECEIVED_PER_TICK) {
            received = receive_from_bound_address(
                hub_socket,
                g_net_data.message_buffer,
                sizeof(char) * NET_MAX_MESSAGE_SIZE);
        }
        else {
            received = false;
        } 

        ++i;
    }
}
