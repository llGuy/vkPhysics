#include "allocators.hpp"
#include "cl_main.hpp"
#include "containers.hpp"
#include "net_meta.hpp"
#include <bits/stdint-uintn.h>
#include <cstring>
#include <vkph_chunk.hpp>
#include <vkph_player.hpp>
#include "cl_net.hpp"
#include <vkph_weapon.hpp>
#include "cl_net_send.hpp"
#include "cl_net_receive.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <string.hpp>
#include <constant.hpp>
#include <cstddef>
#include <ux_menu_game.hpp>
#include "cl_net_meta.hpp"
#include "net_packets.hpp"
#include "net_socket.hpp"

#include <net_debug.hpp>
#include <net_context.hpp>

#include <app.hpp>

namespace cl {

static net::context_t *ctx;
static vkph::listener_t net_listener_id;

static bool started_client = 0;
static bool client_check_incoming_packets = 0;

static uint16_t current_client_id;
static bool simulate_lag = 0; // For debugging purposes.

/*
  The server that we are currently connected to.
*/
static net::game_server_t bound_server;
static net::socket_t tcp_socket;

/*
  When we send the connection request to the server, we need to wait for the handshake
  to arrive before we can start parsing the packets the server is sending us. For instance
  If one of the server's game state snapshot packets arrive before the handshake packet,
  we need to make sure to copy that packet on the heap and push it to this list.
 */
static stack_container_t<net::packet_t> packets_to_unpack;


// Start the client sockets and initialize containers
static void s_start_client(
    vkph::event_start_client_t *data,
    vkph::state_t *state) {
    prepare_receiving();

    memset(ctx->dummy_voxels, vkph::CHUNK_SPECIAL_VALUE, sizeof(ctx->dummy_voxels));
    ctx->init_main_udp_socket(net::GAME_OUTPUT_PORT_CLIENT);
    ctx->clients.init(net::NET_MAX_CLIENT_COUNT);
    started_client = 1;
    state->flags.track_history = 1;
    ctx->accumulated_modifications.init();

    uint32_t sizeof_chunk_mod_pack = sizeof(net::chunk_modifications_t) * net::MAX_PREDICTED_CHUNK_MODIFICATIONS;

    ctx->chunk_modification_allocator.pool_init(
        sizeof_chunk_mod_pack,
        sizeof_chunk_mod_pack * (net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK + 4));

    //s_acc_predicted_modification_init(&merged_recent_modifications, 0);
    ctx->merged_recent_modifications.tick = 0;
    ctx->merged_recent_modifications.acc_predicted_chunk_mod_count = 0;
    ctx->merged_recent_modifications.acc_predicted_modifications = FL_MALLOC(
        net::chunk_modifications_t,
        net::NET_MAX_ACCUMULATED_PREDICTED_CHUNK_MODIFICATIONS_PER_PACK);

    ctx->tag = net::UNINITIALISED_TAG;

    bound_server.tag = net::UNINITIALISED_TAG;
    bound_server.flags.waiting_for_handshake = 0;

    packets_to_unpack.init(20);
}

static void s_handle_packet(vkph::state_t *state, net::packet_t *packet) {
    // LOG_INFOV(
    //     "Received packet of type \"%s\"\n",
    //     net::packet_type_to_str((net::packet_type_t)packet->header.flags.packet_type));

    switch (packet->header.flags.packet_type) {

    case net::PT_PLAYER_JOINED: {
        receive_packet_player_joined(&packet->serialiser, state, ctx);
    } break;

    case net::PT_PLAYER_LEFT: {
        receive_packet_player_left(&packet->serialiser, state, ctx);
    } break;
                    
    case net::PT_GAME_STATE_SNAPSHOT: {
        receive_packet_game_state_snapshot(&packet->serialiser, packet->header.current_tick, state, ctx);
    } break;
                    
    case net::PT_PLAYER_TEAM_CHANGE: {
        receive_player_team_change(&packet->serialiser, state);
    } break;
                    
    case net::PT_PING: {
        receive_ping(&packet->serialiser, state, ctx, &bound_server);
    } break;
                    
    default: {
        LOG_INFOV(
            "Received unidentifiable packet of type %d\n",
            (uint32_t)packet->header.flags.packet_type);
    } break;
    }
}

static void s_check_udp_packets(vkph::state_t *state) {
    static const uint32_t MAX_RECEIVED_PER_TICK = 4;
    uint32_t i = 0;

    net::packet_t packet = net::get_next_packet_udp(ctx);
    while (packet.bytes_received) {
        // Check whether the server tag matches the tag of the packet.
        if (packet.header.tag == bound_server.tag) {
            s_handle_packet(state, &packet);
        }
        else if (bound_server.flags.waiting_for_handshake) {
            if (packets_to_unpack.data_count < packets_to_unpack.max_size) {
                LOG_INFO("Pushing a UDP packet to the list of packets to parse after handshake\n");

                // Create a free list memory block so that we can parse the packet when
                // we received the connection handshake
                uint32_t idx = packets_to_unpack.add();
                net::packet_t *p = &packets_to_unpack[idx];

                uint8_t *membuf = flmalloc<uint8_t>(packet.bytes_received);
                memcpy(membuf, packet.serialiser.data_buffer, p->bytes_received);

                p->bytes_received = packet.bytes_received;
                p->from = packet.from;
                p->header = packet.header;
                p->serialiser.data_buffer = membuf;
                p->serialiser.data_buffer_head = p->serialiser.data_buffer_head;
                p->serialiser.data_buffer_size = p->bytes_received;
            }
        }
        else {
            LOG_INFOV(
                "Received packet from unidentified server of type %d\n",
                (uint32_t)packet.header.flags.packet_type);
        }

        if (i < MAX_RECEIVED_PER_TICK) {
            packet = net::get_next_packet_udp(ctx);
        } else {
            packet.bytes_received = false;
        }

        ++i;
    }
}

static void s_check_tcp_packets(vkph::state_t *state) {
    static const uint32_t MAX_RECEIVED_PER_TICK = 4;
    uint32_t i = 0;

    net::packet_t packet = net::get_next_packet_tcp(ctx->main_tcp_socket, ctx);
    while (packet.bytes_received) {
        if (packet.header.flags.packet_type == net::PT_CONNECTION_HANDSHAKE) {
            receive_packet_connection_handshake(
                &packet.serialiser,
                packet.header.tag,
                state,
                ctx,
                &bound_server);

            bound_server.flags.waiting_for_handshake = 0;

            // Now need to parse the packets we received in between request and handshake
            for (uint32_t i = 0; i < packets_to_unpack.data_count; ++i) {
                auto *packet = &packets_to_unpack[i];
                s_handle_packet(state, packet);
                flfree(packet->serialiser.data_buffer);
            }

            packets_to_unpack.data_count = 0;
        }
        else if (packet.header.tag == bound_server.tag) {
            switch (packet.header.flags.packet_type) {

                // Chunk voxels need to be sent via TCP for more security
            case net::PT_CHUNK_VOXELS: {
                // TODO: Add this to all streamed socket communications: TESTING FOR NOW
                if (packet.bytes_received < packet.header.flags.total_packet_size) {
                    while (packet.bytes_received < packet.header.flags.total_packet_size) {
                        int32_t recv_bytes = net::receive_from_bound_address(
                            ctx->main_tcp_socket,
                            ctx->message_buffer + packet.bytes_received,
                            sizeof(char) * net::NET_MAX_MESSAGE_SIZE - packet.bytes_received);

                        if (recv_bytes) {
                            packet.bytes_received += recv_bytes;
                        }
                    }
                }

                printf("Chunk voxel packet bytes received: packet size is supposed to be %d\n", packet.header.flags.total_packet_size);
                receive_packet_chunk_voxels(&packet.serialiser, state);
            } break;

            }
        }
        else {
            LOG_INFOV(
                "Received packet from unidentified server of type %d\n",
                (uint32_t)packet.header.flags.packet_type);
        }

        if (i < MAX_RECEIVED_PER_TICK) {
            packet = net::get_next_packet_tcp(ctx->main_tcp_socket, ctx);
        } else {
            packet.bytes_received = false;
        }

        ++i;
    }
}

static void s_tick_client(vkph::state_t *state) {
    const app::raw_input_t *input = app::get_raw_input();

#if 1
    if (input->buttons[app::BT_F].instant) {
        simulate_lag = !simulate_lag;

        if (simulate_lag) {LOG_INFO("Simulating lag\n");}
        else {LOG_INFO("Not simulating lag\n");}
    }
#endif

    // Check packets from game server that we are connected to
    if (client_check_incoming_packets) {
        static float elapsed = 0.0f;
        elapsed += app::g_delta_time;
        if (elapsed >= net::NET_CLIENT_COMMAND_OUTPUT_INTERVAL) {
            // Send commands to the server
            net::debug_log("----- Sending client commands to the server at tick %llu\n", ctx->log_file, 0, state->current_tick);
            send_packet_client_commands(state, ctx, &bound_server, simulate_lag);

            elapsed = 0.0f;
        }

        s_check_tcp_packets(state);

        if (client_check_incoming_packets) {
            s_check_udp_packets(state);
        }

        check_if_finished_recv_chunks(state, ctx);
    }
}

static void s_request_connection_to_server(const char *ip, uint32_t ipv4, local_client_info_t *client_info) {
    ctx->main_tcp_socket = net::network_socket_init(net::SP_TCP);
    net::set_socket_recv_buffer_size(ctx->main_tcp_socket, 1024 * 1024);

    if (net::connect_to_address(
            ctx->main_tcp_socket, ip,
            net::GAME_SERVER_LISTENING_PORT, net::SP_TCP)) {

        LOG_INFO("Call to connect was successful!!!\n");

        net::set_socket_blocking_state(ctx->main_tcp_socket, 0);
        client_check_incoming_packets = send_packet_connection_request(ipv4, client_info, ctx, &bound_server);

        bound_server.flags.waiting_for_handshake = 1;

        if (client_check_incoming_packets) {
            bound_server.ipv4_address.port = net::host_to_network_byte_order(net::GAME_OUTPUT_PORT_SERVER);
            LOG_INFO("Successfully sent connection request to server\n");
        }
        else {
            LOG_INFO("Failed to send connection request to server\n");
        } 
    }
}

static void s_net_event_listener(
    void *object,
    vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {

    case vkph::ET_START_CLIENT: {
        auto *data = (vkph::event_start_client_t *)event->data;
        s_start_client(data, state);
        FL_FREE(data);
    } break;

    case vkph::ET_REQUEST_REFRESH_SERVER_PAGE: {
        request_available_servers();
    } break;

    case vkph::ET_REQUEST_TO_JOIN_SERVER: {
        auto *local_meta_client = get_local_meta_client();

        auto *data = (vkph::event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = local_meta_client->username;

        if (data->server_name) {
            auto *available_servers = net::get_available_servers();

            uint32_t *game_server_index = available_servers->name_to_server.get(simple_string_hash(data->server_name));
            if (game_server_index) {
                uint32_t ip_address = available_servers->servers[*game_server_index].ipv4_address.ipv4_address;
                const char *ip_address_str = available_servers->servers[*game_server_index].ip_addr_str;

                s_request_connection_to_server(ip_address_str, ip_address, &client_info);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);
            }
        }
        else if (data->ip_address) {
            { // Create TCP socket
                uint32_t ip_address = str_to_ipv4_int32(data->ip_address, net::GAME_OUTPUT_PORT_SERVER, net::SP_UDP);
                s_request_connection_to_server(data->ip_address, ip_address, &client_info);
            }
        }
        else {
          // Unable to connect to server
        }

        FL_FREE(data);
    } break;

    case vkph::ET_LEAVE_SERVER: {
        if (bound_server.tag != net::UNINITIALISED_TAG) {
            net::destroy_socket(ctx->main_tcp_socket);

            // Send to server message
            send_packet_client_disconnect(state, ctx, &bound_server);
        
            memset(&bound_server, 0, sizeof(bound_server));
            memset(ctx->clients.data, 0, sizeof(net::client_t) * ctx->clients.max_size);
            bound_server.tag = net::UNINITIALISED_TAG;
            ctx->tag = net::UNINITIALISED_TAG;

            client_check_incoming_packets = 0;
        
            LOG_INFO("Disconnecting\n");
        }
    } break;

    case vkph::ET_ATTEMPT_SIGN_UP: {
        auto *data = (vkph::event_attempt_sign_up_t *)event->data;
        request_sign_up(data->username, data->password);
    } break;

    case vkph::ET_ATTEMPT_LOGIN: {
        auto *data = (vkph::event_attempt_login_t *)event->data;
        request_login(data->username, data->password);
    } break;

    case vkph::ET_SEND_SERVER_TEAM_SELECT_REQUEST: {
        auto *data = (vkph::event_send_server_team_select_request_t *)event->data;
        send_packet_team_select_request(data->color, state, ctx, &bound_server);
    } break;

    case vkph::ET_CLOSED_WINDOW: {
        notify_meta_disconnection();
    } break;

    }
}

void init_net(vkph::state_t *state) {
    ctx = flmalloc<net::context_t>();

    net_listener_id = vkph::set_listener_callback(&s_net_event_listener, state);

    vkph::subscribe_to_event(vkph::ET_START_CLIENT, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_REQUEST_TO_JOIN_SERVER, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_LEAVE_SERVER, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_REQUEST_REFRESH_SERVER_PAGE, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_ATTEMPT_SIGN_UP, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_CLOSED_WINDOW, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_ATTEMPT_LOGIN, net_listener_id);
    vkph::subscribe_to_event(vkph::ET_SEND_SERVER_TEAM_SELECT_REQUEST, net_listener_id);

    net::init_socket_api();

    ctx->message_buffer = FL_MALLOC(char, net::NET_MAX_MESSAGE_SIZE);

    init_meta_connection();

    auto *available_servers = net::get_available_servers();

    available_servers->server_count = 0;
    available_servers->servers = FL_MALLOC(net::game_server_t, net::NET_MAX_AVAILABLE_SERVER_COUNT);
    memset(available_servers->servers, 0, sizeof(net::game_server_t) * net::NET_MAX_AVAILABLE_SERVER_COUNT);
    available_servers->name_to_server.init();
}

void tick_net(vkph::state_t *state) {
    // check_incoming_meta_server_packets(events);
    // Check for meta server packets
    check_meta_request_status_and_handle();

    s_tick_client(state);
}

bool is_connected_to_server() {
    return bound_server.tag != net::UNINITIALISED_TAG;
}

uint16_t &get_local_client_index() {
    return current_client_id;
}

}
