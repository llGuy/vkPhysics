#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "n_internal.hpp"
#include <common/log.hpp>
#include <common/event.hpp>
#include <common/string.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>
#include <glm/gtx/string_cast.hpp>

static float client_command_output_interval = 1.0f / 25.0f;
static float server_snapshot_output_interval = 1.0f / 20.0f;

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

static network_address_t bound_server_address = {};

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
            clients.data[client_id].chunks_to_wait_for = handshake.loaded_chunk_count;

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

#include <renderer/input.hpp>

// Debugging!
static bool simulate_lag = 0;

static void s_send_commands_to_server() {
    player_t *p = get_player(current_client_id);

    // Means that world hasn't been initialised yet (could be timing issues when submitting ENTER_SERVER
    // event and send commands interval, so just to make sure, check that player is not NULL)
    if (p) {
        if (simulate_lag) {
            p->cached_player_action_count = 0;
        }
        else {
            client_t *c = &clients[p->client_id];

            packet_player_commands_t packet = {};
            packet.did_correction = c->waiting_on_correction;
            packet.command_count = (uint8_t)p->cached_player_action_count;
            packet.actions = LN_MALLOC(player_actions_t, packet.command_count);

            for (uint32_t i = 0; i < packet.command_count; ++i) {
                packet.actions[i].bytes = p->cached_player_actions[i].bytes;
                packet.actions[i].dmouse_x = p->cached_player_actions[i].dmouse_x;
                packet.actions[i].dmouse_y = p->cached_player_actions[i].dmouse_y;
                packet.actions[i].dt = p->cached_player_actions[i].dt;
            }

            packet.ws_final_position = p->ws_position;
            packet.ws_final_view_direction = p->ws_view_direction;
            packet.ws_final_up_vector = p->ws_up_vector;

            packet_header_t header = {};
            header.current_tick = get_current_tick();
            header.current_packet_count = current_packet;
            header.client_id = current_client_id;
            header.flags.packet_type = PT_CLIENT_COMMANDS;
            header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_player_commands_size(&packet);

            serialiser_t serialiser = {};
            serialiser.init(header.flags.total_packet_size);
            n_serialise_packet_header(&header, &serialiser);
            n_serialise_player_commands(&packet, &serialiser);

            s_send_to(&serialiser, bound_server_address);
    
            p->cached_player_action_count = 0;

            c->waiting_on_correction = 0;
        }
    }
}

static void s_process_game_state_snapshot(
    serialiser_t *serialiser,
    uint64_t received_tick,
    event_submissions_t *events) {
    packet_game_state_snapshot_t packet = {};
    n_deserialise_game_state_snapshot(&packet, serialiser);

    for (uint32_t i = 0; i < packet.player_data_count; ++i) {
        player_snapshot_t *snapshot = &packet.player_snapshots[i];

        if (snapshot->client_id == current_client_id) {
            client_t *c = &clients[snapshot->client_id];
            player_t *p = get_player(snapshot->client_id);

            // TODO: Watch out for this:
            if (snapshot->client_needs_to_correct) {
                LOG_INFOV("Did correction at tick %llu!\n", (unsigned long long)received_tick);

                get_current_tick() = received_tick;
                
                // Do correction!
                p->ws_position = snapshot->ws_position;
                p->ws_view_direction = snapshot->ws_view_direction;
                p->ws_up_vector = snapshot->ws_up_vector;
                p->cached_player_action_count = 0;

                // Basically says that the client just did a correction - set correction flag on next packet sent to server
                c->waiting_on_correction = 1;
            }
        }
        else {
            //client_t *c = &clients[snapshot->client_id];
            player_t *p = get_player(snapshot->client_id);
            
            p->remote_snapshots.push_item(snapshot);
            
            /*p->ws_position = snapshot->ws_position;
            p->ws_view_direction = snapshot->ws_view_direction;
            p->ws_up_vector = snapshot->ws_up_vector;*/
        }
    }
}

static void s_process_chunk_voxels(
    serialiser_t *serialiser,
    event_submissions_t *events) {
    packet_chunk_voxels_t *packet = LN_MALLOC(packet_chunk_voxels_t, 1);
    n_deserialise_packet_chunk_voxels(packet, serialiser);

    event_chunk_voxel_packet_t *data = FL_MALLOC(event_chunk_voxel_packet_t, 1);
    data->packet = packet;

    LOG_INFO("Received chunk values packet\n");

    submit_event(ET_CHUNK_VOXEL_PACKET, data, events);

    client_t *c = &clients[current_client_id];
    c->chunks_to_wait_for -= packet->chunk_in_packet_count;

    if (c->chunks_to_wait_for == 0) {
        submit_event(ET_FINISHED_RECEIVING_INITIAL_CHUNK_DATA, NULL, events);
    }
    else {
        submit_event(ET_STARTED_RECEIVING_INITIAL_CHUNK_DATA, NULL, events);
    }
}

void tick_client(
    event_submissions_t *events) {
    raw_input_t *input = get_raw_input();
    if (input->buttons[BT_F].instant) {
        simulate_lag = !simulate_lag;

        if (simulate_lag) {LOG_INFO("Simulating lag\n");}
        else {LOG_INFO("Not simulating lag\n");}
    }
    
    if (client_check_incoming_packets) {
        static float elapsed = 0.0f;
        elapsed += logic_delta_time();
        if (elapsed >= client_command_output_interval) {
            // Send commands to the server
            s_send_commands_to_server();

            elapsed = 0.0f;
        }

        network_address_t received_address = {};
        int32_t received = n_receive_from(
            main_udp_socket,
            message_buffer,
            sizeof(char) * MAX_MESSAGE_SIZE,
            &received_address);

        // In future, separate thread will be capturing all these packets
        static const uint32_t MAX_RECEIVED_PER_TICK = 4;
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

            case PT_GAME_STATE_SNAPSHOT: {
                s_process_game_state_snapshot(
                    &in_serialiser,
                    header.current_tick,
                    events);
            } break;

            case PT_CHUNK_VOXELS: {
                s_process_chunk_voxels(
                    &in_serialiser,
                    events);
            } break;

            default: {
                LOG_INFO("Received unidentifiable packet\n");
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

static bool s_send_handshake(
    uint16_t client_id,
    event_new_player_t *player_info,
    uint32_t loaded_chunk_count) {
    packet_connection_handshake_t connection_handshake = {};
    connection_handshake.loaded_chunk_count = loaded_chunk_count;
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
        LOG_INFOV("Sent handshake to client: %s\n", c->name);
        return 1;
    }
    else {
        LOG_INFOV("Failed to send handshake to client: %s\n", c->name);
        return 0;
    }
}

static constexpr uint32_t maximum_chunks_per_packet() {
    return ((65507 - sizeof(uint32_t)) / (sizeof(int16_t) * 3 + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH));
}

static void s_send_game_state_to_new_client(
    uint16_t client_id,
    event_new_player_t *player_info) {
    uint32_t loaded_chunk_count = 0;
    chunk_t **chunks = get_active_chunks(&loaded_chunk_count);

    if (s_send_handshake(
        client_id,
        player_info,
        loaded_chunk_count)) {
        client_t *client = clients.get(client_id);

        // Send chunk information
        uint32_t max_chunks_per_packet = maximum_chunks_per_packet();
        LOG_INFOV("Maximum chunks per packet: %i\n", max_chunks_per_packet);

        voxel_chunk_values_t *voxel_chunks = LN_MALLOC(voxel_chunk_values_t, loaded_chunk_count);

        uint32_t count = 0;
        for (uint32_t i = 0; i < loaded_chunk_count; ++i) {
            chunk_t *c = chunks[i];
            if (c) {
                voxel_chunks[count].x = c->chunk_coord.x;
                voxel_chunks[count].y = c->chunk_coord.y;
                voxel_chunks[count].z = c->chunk_coord.z;

                voxel_chunks[count].voxel_values = c->voxels;

                ++count;
            }
        }

        loaded_chunk_count = count;
        
        uint32_t full_packets = loaded_chunk_count / max_chunks_per_packet;
        uint32_t chunks_left = loaded_chunk_count;

        serialiser_t serialiser = {};
        serialiser.init(max_chunks_per_packet * (3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH));

        packet_header_t header = {};
        header.flags.packet_type = PT_CHUNK_VOXELS;
        header.flags.total_packet_size = max_chunks_per_packet * (3 * sizeof(int16_t) + CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);
        header.current_tick = get_current_tick();
        header.current_packet_count = current_packet;
        
        voxel_chunk_values_t *current = voxel_chunks;
        for (uint32_t i = 0; i < full_packets; ++i) {
            packet_chunk_voxels_t packet = {};
            packet.chunk_in_packet_count = max_chunks_per_packet;
            packet.values = current;

            n_serialise_packet_header(&header, &serialiser);
            n_serialise_packet_chunk_voxels(&packet, &serialiser);

            if (s_send_to(&serialiser, client->address)) {
                LOG_INFOV("Sent data for %i chunks\n", max_chunks_per_packet);
            }
            else {
                LOG_ERROR("Failed to send chunks\n");
            }

            current += max_chunks_per_packet;
            chunks_left -= max_chunks_per_packet;
            // Reset serialiser for next packet
            serialiser.data_buffer_head = 0;

        }

        if (chunks_left) {
            packet_chunk_voxels_t packet = {};
            packet.chunk_in_packet_count = chunks_left;
            packet.values = current;

            n_serialise_packet_header(&header, &serialiser);
            n_serialise_packet_chunk_voxels(&packet, &serialiser);

            s_send_to(&serialiser, client->address);

            LOG_INFOV("Sent data for %i chunks\n", chunks_left);
            
            current += max_chunks_per_packet;
            chunks_left -= max_chunks_per_packet;
            // Reset serialiser for next packet
            serialiser.data_buffer_head = 0;
        }
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
    client->received_first_commands_packet = 0;

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
    (void)serialiser;
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

static void s_process_client_commands(
    serialiser_t *serialiser,
    uint16_t client_id,
    event_submissions_t *events) {
    player_t *p = get_player(client_id);

    if (p) {
        client_t *c = &clients[p->client_id];

        c->received_first_commands_packet = 1;

        packet_player_commands_t commands = {};
        n_deserialise_player_commands(&commands, serialiser);

        if (commands.did_correction) {
            LOG_INFOV("Did correction: %s\n", glm::to_string(p->ws_position).c_str());
            c->waiting_on_correction = 0;
        }
        
        // Only process client commands if we are not waiting on a correction
        if (!c->waiting_on_correction) {
            for (uint32_t i = 0; i < commands.command_count; ++i) {
                push_player_actions(p, &commands.actions[i]);
            }

            c->ws_predicted_position = commands.ws_final_position;
            c->ws_predicted_view_direction = commands.ws_final_view_direction;
            c->ws_predicted_up_vector = commands.ws_final_up_vector;
        }
    }
    else {
        // There is a problem
        LOG_ERROR("Player was not initialised yet, cannot process client commands!\n");
    }
}

static bool s_check_if_client_has_to_correct(
    player_t *p,
    client_t *c) {
    vector3_t dposition = glm::abs(p->ws_position - c->ws_predicted_position);
    vector3_t ddirection = glm::abs(p->ws_view_direction - c->ws_predicted_view_direction);
    vector3_t dup = glm::abs(p->ws_up_vector - c->ws_predicted_up_vector);

    float precision = 0.000001f;
    bool incorrect_position = 0;
    if (dposition.x >= precision || dposition.y >= precision || dposition.z >= precision) {
        incorrect_position = 1;

        LOG_INFOV("Position is wrong: %s => %s\n", glm::to_string(c->ws_predicted_position).c_str(), glm::to_string(p->ws_position).c_str());
    }

    bool incorrect_direction = 0;
    if (ddirection.x >= precision || ddirection.y >= precision || ddirection.z >= precision) {
        incorrect_direction = 1;

        LOG_INFOV("Direction is wrong: %s => %s\n", glm::to_string(c->ws_predicted_view_direction).c_str(), glm::to_string(p->ws_view_direction).c_str());
    }

    bool incorrect_up = 0;
    if (dup.x >= precision || dup.y >= precision || dup.z >= precision) {
        incorrect_up = 1;

        LOG_INFOV("Up is wrong: %s => %s\n", glm::to_string(c->ws_predicted_up_vector).c_str(), glm::to_string(p->ws_up_vector).c_str());
    }

    return incorrect_position || incorrect_direction || incorrect_up;
}

static void s_dispatch_game_state_snapshot() {
    packet_game_state_snapshot_t packet = {};
    packet.player_data_count = 0;
    packet.player_snapshots = LN_MALLOC(player_snapshot_t, clients.data_count);

    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];

        if (c->initialised && c->received_first_commands_packet) {
            // Check if the data that the client predicted was correct, if not, force client to correct position
            // Until server is sure that the client has done a correction, server will not process this client's commands
            player_snapshot_t *snapshot = &packet.player_snapshots[packet.player_data_count];
            snapshot->flags = 0;

            player_t *p = get_player(c->client_id);
            bool has_to_correct = s_check_if_client_has_to_correct(p, c);
            if (has_to_correct) {
                LOG_INFOV("Client needs to do correction: tick %i\n", (int32_t)get_current_tick());
                snapshot->client_needs_to_correct = has_to_correct;
                c->waiting_on_correction = 1;
            }

            snapshot->client_id = c->client_id;
            snapshot->ws_position = p->ws_position;
            snapshot->ws_view_direction = p->ws_view_direction;
            snapshot->ws_up_vector = p->ws_up_vector;

            ++packet.player_data_count;
        }
    }

    packet_header_t header = {};
    header.current_tick = get_current_tick();
    header.current_packet_count = current_packet;
    // Don't need to fill this
    header.client_id = 0;
    header.flags.packet_type = PT_GAME_STATE_SNAPSHOT;
    header.flags.total_packet_size = n_packed_packet_header_size() + n_packed_game_state_snapshot_size(&packet);

    serialiser_t serialiser = {};
    serialiser.init(header.flags.total_packet_size);

    n_serialise_packet_header(&header, &serialiser);
    n_serialise_game_state_snapshot(&packet, &serialiser);

    for (uint32_t i = 0; i < clients.data_count; ++i) {
        client_t *c = &clients[i];

        if (c->initialised && c->received_first_commands_packet) {
            s_send_to(&serialiser, c->address);
        }
    }
}

void tick_server(
    event_submissions_t *events) {
    // In future, have a separate thread do this kind of stuff
    static float elapsed = 0.0f;
    elapsed += logic_delta_time();
    if (elapsed >= server_snapshot_output_interval) {
        // Send commands to the server
        s_dispatch_game_state_snapshot();

        elapsed = 0.0f;
    }

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

            case PT_CLIENT_COMMANDS: {
                s_process_client_commands(
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
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_disconnect_to_server();
        
            memset(&bound_server_address, 0, sizeof(bound_server_address));
            memset(clients.data, 0, sizeof(client_t) * clients.max_size);

            client_check_incoming_packets = 0;
        
            LOG_INFO("Disconnecting\n");
        }
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

bool connected_to_server() {
    return bound_server_address.ipv4_address != 0;
}

float server_snapshot_interval() {
    return server_snapshot_output_interval;
}

float client_command_interval() {
    return client_command_output_interval;
}
