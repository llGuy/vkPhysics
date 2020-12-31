#include "cl_main.hpp"
#include "net_meta.hpp"
#include <vkph_chunk.hpp>
#include <vkph_player.hpp>
#include <vkph_weapon.hpp>
#include "nw_client.hpp"
#include "wd_interp.hpp"
#include "wd_predict.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <string.hpp>
#include "nw_client_meta.hpp"
#include <constant.hpp>
#include <cstddef>
#include <ux_menu_game.hpp>

#include <net_debug.hpp>
#include <net_context.hpp>

#include <app.hpp>

namespace cl {

static net::context_t *ctx;

static bool started_client = 0;
static bool client_check_incoming_packets = 0;

static uint16_t current_client_id;

static net::address_t bound_server_address = {};

// Start the client sockets and initialize containers
static void s_start_client(
    vkph::event_start_client_t *data,
    vkph::state_t *state) {
    still_receiving_chunk_packets = 0;
    chunks_to_receive = 0;

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
}

#include <app.hpp>



static void s_tick_client(vkph::state_t *state) {
    s_check_incominstate_server_packets(state);
}

// PT_CONNECTION_REQUEST
static void s_send_packet_connection_request(
    uint32_t ip_address,
    local_client_info_t *info) {
    bound_server_address.port = net::host_to_network_byte_order(net::GAME_OUTPUT_PORT_SERVER);
    bound_server_address.ipv4_address = ip_address;

    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_connection_request_t request = {};
    request.name = info->name;
    
    net::packet_header_t header = {};
    header.flags.packet_type = net::PT_CONNECTION_REQUEST;
    header.flags.total_packet_size = header.size() + request.size();
    header.current_packet_count = ctx->current_packet;

    header.serialise(&serialiser);
    request.serialise(&serialiser);

    if (ctx->main_udp_send_to(&serialiser, bound_server_address)) {
        LOG_INFO("Success sent connection request\n");
        client_check_incoming_packets = 1;
    }
    else {
        LOG_ERROR("Failed to send connection request\n");
    }
}

// PT_TEAM_SELECT_REQUEST
static void s_send_packet_team_select_request(vkph::team_color_t color, const vkph::state_t *state) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = net::PT_TEAM_SELECT_REQUEST;
    header.flags.total_packet_size = header.size() + sizeof(uint32_t);

    header.serialise(&serialiser);
    serialiser.serialise_uint32((uint32_t)color);

    ctx->main_udp_send_to(&serialiser, bound_server_address);
}

// PT_CLIENT_DISCONNECT
static void s_send_packet_client_disconnect(const vkph::state_t *state) {
    serialiser_t serialiser = {};
    serialiser.init(100);

    net::packet_header_t header = {};
    header.current_tick = state->current_tick;
    header.current_packet_count = ctx->current_packet;
    header.client_id = current_client_id;
    header.flags.packet_type = net::PT_CLIENT_DISCONNECT;
    header.flags.total_packet_size = header.size();

    header.serialise(&serialiser);
    
    ctx->main_udp_send_to(&serialiser, bound_server_address);
}

static vkph::listener_t net_listener_id;

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
                uint32_t ip_address = available_servers->servers[*game_server_index].ipv4_address;
            
                s_send_packet_connection_request(ip_address, &client_info);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);
            }
        }
        else if (data->ip_address) {
            uint32_t ip_address = str_to_ipv4_int32(data->ip_address, net::GAME_OUTPUT_PORT_SERVER, net::SP_UDP);
            s_send_packet_connection_request(ip_address, &client_info);
        }
        else {
            
        }

        FL_FREE(data);
    } break;

    case vkph::ET_LEAVE_SERVER: {
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_packet_client_disconnect(state);
        
            memset(&bound_server_address, 0, sizeof(bound_server_address));
            memset(ctx->clients.data, 0, sizeof(net::client_t) * ctx->clients.max_size);

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

        s_send_packet_team_select_request(data->color, state);
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
    return bound_server_address.ipv4_address != 0;
}

uint16_t &get_local_client_index() {
    return current_client_id;
}

}
