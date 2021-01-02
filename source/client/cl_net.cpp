#include "cl_main.hpp"
#include "net_meta.hpp"
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
        elapsed += delta_time();
        if (elapsed >= net::NET_CLIENT_COMMAND_OUTPUT_INTERVAL) {
            // Send commands to the server
            net::debug_log("----- Sending client commands to the server at tick %llu\n", ctx->log_file, 0, state->current_tick);
            send_packet_client_commands(state, ctx, &bound_server, simulate_lag);

            elapsed = 0.0f;
        }

        net::address_t received_address = {};
        int32_t received = ctx->main_udp_recv_from(
            ctx->message_buffer,
            sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
            &received_address);

        // In future, separate thread will be capturing all these packets
        static const uint32_t MAX_RECEIVED_PER_TICK = 4;
        uint32_t i = 0;

        while (received) {
            serialiser_t in_serialiser = {};
            in_serialiser.data_buffer = (uint8_t *)ctx->message_buffer;
            in_serialiser.data_buffer_size = received;

            net::packet_header_t header = {};
            header.deserialise(&in_serialiser);

            if (header.flags.packet_type == net::PT_CONNECTION_HANDSHAKE) {
                if (bound_server.tag == net::UNINITIALISED_TAG) {
                    receive_packet_connection_handshake(&in_serialiser, header.tag, state, ctx, &bound_server);
                }
            }
            else {
                // Check whether the server tag matches the tag of the packet.
                if (header.tag == bound_server.tag) {
                    switch (header.flags.packet_type) {

                    case net::PT_PLAYER_JOINED: {
                        receive_packet_player_joined(&in_serialiser, state, ctx);
                    } break;

                    case net::PT_PLAYER_LEFT: {
                        receive_packet_player_left(&in_serialiser, state, ctx);
                    } break;

                    case net::PT_GAME_STATE_SNAPSHOT: {
                        receive_packet_game_state_snapshot(&in_serialiser, header.current_tick, state, ctx);
                    } break;

                    case net::PT_CHUNK_VOXELS: {
                        receive_packet_chunk_voxels(&in_serialiser, state);
                    } break;

                    case net::PT_PLAYER_TEAM_CHANGE: {
                        receive_player_team_change(&in_serialiser, state);
                    } break;

                    case net::PT_PING: {
                        receive_ping(&in_serialiser, state, ctx, &bound_server);
                    } break;

                    default: {
                        LOG_INFO("Received unidentifiable packet\n");
                    } break;
                    }
                }
                else {
                    LOG_INFO("Received packet from unidentified server\n");
                }
            }

            if (i < MAX_RECEIVED_PER_TICK) {
                received = ctx->main_udp_recv_from(
                    ctx->message_buffer,
                    sizeof(char) * net::NET_MAX_MESSAGE_SIZE,
                    &received_address);
            }
            else {
                received = false;
            } 

            ++i;
        }

        check_if_finished_recv_chunks(state, ctx);
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
            
                client_check_incoming_packets = send_packet_connection_request(ip_address, &client_info, ctx, &bound_server);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);
            }
        }
        else if (data->ip_address) {
            uint32_t ip_address = str_to_ipv4_int32(data->ip_address, net::GAME_OUTPUT_PORT_SERVER, net::SP_UDP);
            client_check_incoming_packets = send_packet_connection_request(ip_address, &client_info, ctx, &bound_server);
        }
        else {
            // Unable to connect to server
        }

        FL_FREE(data);
    } break;

    case vkph::ET_LEAVE_SERVER: {
        if (bound_server.tag != net::UNINITIALISED_TAG) {
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
