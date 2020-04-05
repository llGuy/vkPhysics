#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "n_internal.hpp"
#include <common/log.hpp>
#include <common/event.hpp>
#include <common/string.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>

#define MAX_MESSAGE_SIZE 40000

static char *message_buffer;
static socket_t main_udp_socket;
static uint64_t current_packet;

#define GAME_OUTPUT_PORT_CLIENT 6001
#define GAME_OUTPUT_PORT_SERVER 6000

static void s_main_udp_socket_init(
    uint16_t output_port) {
    current_packet = 0;

    main_udp_socket = n_network_socket_init(SP_UDP);
    network_address_t address = {};
    address.port = n_host_to_network_byte_order(output_port);
    n_bind_network_socket_to_port(main_udp_socket, address);
    n_set_socket_to_non_blocking_mode(main_udp_socket);
}

static bool s_send_to(
    serialiser_t *serialiser,
    network_address_t address) {
    ++current_packet;
    return n_send_to(main_udp_socket, address, (char *)serialiser->data_buffer, serialiser->data_buffer_head);
}

static bool started_client = 0;
static uint16_t current_client_id;
static bool client_check_incoming_packets = 0;

#define MAX_CLIENTS 50
static stack_container_t<client_t> clients;

static void s_start_client() {
    s_main_udp_socket_init(GAME_OUTPUT_PORT_CLIENT);

    clients.init(MAX_CLIENTS);
    
    started_client = 1;
}

static network_address_t bound_server_address;

static void s_process_connection_handshake(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_connection_handshake_t handshake = {};
    n_deserialise_connection_handshake(&handshake, serialiser);

    LOG_INFOV("Received handshake, there are %i players\n", handshake.player_count);

    event_enter_server_t *data = FL_MALLOC(event_enter_server_t, 1);
    data->info_count = handshake.player_count;
    data->infos = FL_MALLOC(player_init_info_t, data->info_count);

    int32_t highest_client_index = -1;
    
    // Add all the players
    for (uint32_t i = 0; i < handshake.player_count; ++i) {
        if (highest_client_index < (int32_t)handshake.player_infos[i].client_id) {
            highest_client_index = handshake.player_infos[i].client_id;
        }

        uint16_t client_id = handshake.player_infos[i].client_id;

        clients.data[client_id].client_id = client_id;
        clients.data[client_id].name = handshake.player_infos[i].name;
        clients.data[client_id].initialised = 1;

        data->infos[i].client_data = &clients.data[client_id];
        data->infos[i].ws_position = handshake.player_infos[i].ws_position;
        data->infos[i].ws_view_direction = handshake.player_infos[i].ws_view_direction;
        data->infos[i].ws_up_vector = handshake.player_infos[i].ws_up_vector;
        data->infos[i].default_speed = handshake.player_infos[i].default_speed;
        data->infos[i].is_local = handshake.player_infos[i].is_local;

        if (data->infos[i].is_local) {
            data->local_client_id = clients.data[i].client_id;
            current_client_id = data->local_client_id;
        }
    }

    // Don't keep track for client
    //clients.data_count = (uint16_t)highest_client_index + 1;

    submit_event(ET_ENTER_SERVER, data, events);
}

static void s_process_player_joined(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    packet_player_joined_t packet = {};
    n_deserialise_player_joined(&packet, in_serialiser);

    LOG_INFOV("%s joined the game\n", packet.player_info.name);

    event_new_player_t *new_player = FL_MALLOC(event_new_player_t, 1);
    client_t *c = &clients.data[packet.player_info.client_id];
    c->name = packet.player_info.name;
    c->client_id = packet.player_info.client_id;

    new_player->info.client_data = c;
    new_player->info.ws_position = packet.player_info.ws_position;
    new_player->info.ws_view_direction = packet.player_info.ws_view_direction;
    new_player->info.ws_up_vector = packet.player_info.ws_up_vector;
    new_player->info.default_speed = packet.player_info.default_speed;
    new_player->info.is_local = 0;

    submit_event(ET_NEW_PLAYER, new_player, events);
}

static void s_process_player_left(
    serialiser_t *in_serialiser,
    event_submissions_t *events) {
    uint16_t disconnected_client = in_serialiser->deserialise_uint16();

    clients.data[disconnected_client].initialised = 0;

    event_player_disconnected_t *data = FL_MALLOC(event_player_disconnected_t, 1);
    data->client_id = disconnected_client;
    submit_event(ET_PLAYER_DISCONNECTED, data, events);

    LOG_INFO("Player disconnected\n");
}

void tick_client(
    event_submissions_t *events) {
    if (client_check_incoming_packets) {
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
                return;
            } break;

            case PT_PLAYER_JOINED: {
                s_process_player_joined(
                    &in_serialiser,
                    events);
            } break;

            case PT_PLAYER_LEFT: {
                s_process_player_left(
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
}

static void s_send_connect_request_to_server(
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
    header.current_packet_count = current_packet;

    n_serialise_packet_header(&header, &serialiser);
    n_serialise_connection_request(&request, &serialiser);

    if (s_send_to(&serialiser, bound_server_address)) {
        LOG_INFO("Success sent connection request\n");
        client_check_incoming_packets = 1;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
    }
}

static void s_send_disconnect_to_server() {
    serialiser_t serialiser = {};
    serialiser.init(100);

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = n_packed_packet_header_size();

    n_serialise_packet_header(&header, &serialiser);
    
    s_send_to(&serialiser, bound_server_address);
}

static bool started_server = 0;

static void s_start_server() {
    s_main_udp_socket_init(GAME_OUTPUT_PORT_SERVER);

    clients.init(MAX_CLIENTS);

    started_server = 1;
}

static void s_send_game_state_to_new_client(
    uint16_t client_id,
    event_new_player_t *player_info) {
    packet_connection_handshake_t connection_handshake = {};
    connection_handshake.player_infos = LN_MALLOC(full_player_info_t, clients.data_count);

    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *client = clients.get(i);
        if (client->initialised) {
            full_player_info_t *info = &connection_handshake.player_infos[connection_handshake.player_count];

            if (i == client_id) {
                info->name = client->name;
                info->client_id = client->client_id;
                info->ws_position = player_info->info.ws_position;
                info->ws_view_direction = player_info->info.ws_view_direction;
                info->ws_up_vector = player_info->info.ws_up_vector;
                info->default_speed = player_info->info.default_speed;
                info->is_local = 1;
            }
            else {
                player_t *p = get_player(i);
                info->name = client->name;
                info->client_id = client->client_id;
                info->ws_position = p->ws_position;
                info->ws_view_direction = p->ws_view_direction;
                info->ws_up_vector = p->ws_up_vector;
                info->default_speed = p->default_speed;
                info->is_local = 0;
            }
            
            ++connection_handshake.player_count;
        }
    }

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_connection_handshake_size(&connection_handshake);
    header.flags.packet_type = PT_CONNECTION_HANDSHAKE;

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    n_serialise_packet_header(
        &header,
        &serialiser);
    
    n_serialise_connection_handshake(
        &connection_handshake,
        &serialiser);

    client_t *c = clients.get(client_id);

    if (s_send_to(&serialiser, c->address)) {
        LOG_INFOV("Send handshake to client: %s\n", c->name);
    }
    else {
        LOG_INFOV("Failed to send handshake to client: %s\n", c->name);
    }
}

static void s_inform_all_players_on_newcomer(
    event_new_player_t *info) {
    packet_player_joined_t packet = {};
    packet.player_info.name = info->info.client_data->name;
    packet.player_info.client_id = info->info.client_data->client_id;
    packet.player_info.ws_position = info->info.ws_position;
    packet.player_info.ws_view_direction = info->info.ws_view_direction;
    packet.player_info.ws_up_vector = info->info.ws_up_vector;
    packet.player_info.default_speed = info->info.default_speed;
    packet.player_info.is_local = 0;

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_player_joined_size(&packet);
    header.flags.packet_type = PT_PLAYER_JOINED;
    
    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);
    
    n_serialise_packet_header(&header, &serialiser);
    n_serialise_player_joined(&packet, &serialiser);
    
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        if (i != packet.player_info.client_id) {
            client_t *c = clients.get(i);
            if (c->initialised) {
                s_send_to(&serialiser, c->address);
            }
        }
    }
}

static void s_process_connection_request(
    serialiser_t *serialiser,
    network_address_t address,
    event_submissions_t *events) {
    packet_connection_request_t request = {};
    n_deserialise_connection_request(&request, serialiser);

    uint32_t client_id = clients.add();
    client_t *client = clients.get(client_id);
    
    client->initialised = 1;
    client->client_id = client_id;
    client->name = create_fl_string(request.name);
    client->address = address;

    event_new_player_t *event_data = FL_MALLOC(event_new_player_t, 1);
    event_data->info.client_data = client;
    // Need to calculate a random position
    // TODO: In future, make it so that there is like some sort of startup screen when joining a server (like choose team, etc..)
    event_data->info.ws_position = vector3_t(0.0f);
    event_data->info.ws_view_direction = vector3_t(1.0f, 0.0f, 0.0f);
    event_data->info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);
    event_data->info.default_speed = 10.0f;
    event_data->info.is_local = 0;
    
    submit_event(ET_NEW_PLAYER, event_data, events);

    // Send game state to new player
    s_send_game_state_to_new_client(client_id, event_data);
    // Dispatch to all players newly joined player information
    s_inform_all_players_on_newcomer(event_data);
}

static void s_process_client_disconnect(
    serialiser_t *serialiser,
    uint16_t client_id,
    event_submissions_t *events) {
    LOG_INFO("Client disconnected\n");

    clients[client_id].initialised = 0;
    clients.remove(client_id);

    event_player_disconnected_t *data = FL_MALLOC(event_player_disconnected_t, 1);
    data->client_id = client_id;
    submit_event(ET_PLAYER_DISCONNECTED, data, events);

    serialiser_t out_serialiser = {};
    out_serialiser.init(100);
    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    header.flags.packet_type = PT_PLAYER_LEFT;
    header.flags.total_packet_size = n_packed_packet_header_size() + sizeof(uint16_t);

    n_serialise_packet_header(&header, &out_serialiser);
    out_serialiser.serialise_uint16(client_id);
    
    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];
        if (c->initialised) {
            s_send_to(&out_serialiser, c->address);
        }
    }
}

void tick_server(
    event_submissions_t *events) {
    // In future, have a separate thread capturing these
    for (uint32_t i = 0; i < clients.data_count + 1; ++i) {
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
                    received_address,
                    events);
            } break;

            case PT_CLIENT_DISCONNECT: {
                s_process_client_disconnect(
                    &in_serialiser,
                    header.client_id,
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

static listener_t net_listener_id;

static void s_net_event_listener(
    void *object,
    event_t *event) {
    switch (event->type) {

    case ET_START_CLIENT: {
        s_start_client();
    } break;

    case ET_START_SERVER: {
        s_start_server();
    } break;

    case ET_REQUEST_TO_JOIN_SERVER: {
        event_data_request_to_join_server_t *data = (event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = data->client_name;
        s_send_connect_request_to_server(data->ip_address, &client_info);

        FL_FREE(data);
    } break;

    case ET_LEAVE_SERVER: {
        // Send to server message
        s_send_disconnect_to_server();
        
        memset(&bound_server_address, 0, sizeof(bound_server_address));
        memset(clients.data, 0, sizeof(client_t) * clients.max_size);
        
        LOG_INFO("Disconnecting\n");
    } break;

    }
}

void net_init(
    event_submissions_t *events) {
    net_listener_id = set_listener_callback(
        &s_net_event_listener,
        NULL,
        events);

    subscribe_to_event(ET_START_CLIENT, net_listener_id, events);
    subscribe_to_event(ET_START_SERVER, net_listener_id, events);
    subscribe_to_event(ET_REQUEST_TO_JOIN_SERVER, net_listener_id, events);
    subscribe_to_event(ET_LEAVE_SERVER, net_listener_id, events);

    n_socket_api_init();

    message_buffer = FL_MALLOC(char, MAX_MESSAGE_SIZE);
}
