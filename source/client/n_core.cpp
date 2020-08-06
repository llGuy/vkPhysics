#include "cl_main.hpp"
#include <common/game.hpp>
#include "wd_interp.hpp"
#include <common/player.hpp>
#include <common/socket.hpp>
#include "net.hpp"
#include "wd_predict.hpp"
#include <renderer/renderer.hpp>
#include "n_internal.hpp"
#include <common/log.hpp>
#include <common/event.hpp>
#include <common/string.hpp>
#include <common/hub_packet.hpp>
#include <common/containers.hpp>
#include <common/allocators.hpp>
#include <cstddef>
#include <cstring>
#include <glm/gtx/string_cast.hpp>

void tick_net(
    event_submissions_t *events) {
    s_check_incoming_hub_server_packets(events);

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
    event_t *event,
    event_submissions_t *events) {
    switch (event->type) {

    case ET_START_CLIENT: {
        event_start_client_t *data = (event_start_client_t *)event->data;
        s_start_client(data);
        FL_FREE(data);
    } break;

    case ET_START_SERVER: {
        event_start_server_t *data = (event_start_server_t *)event->data;
        s_start_server(data);

        FL_FREE(data);
    } break;

    case ET_REQUEST_REFRESH_SERVER_PAGE: {
        s_request_available_servers();
    } break;

    case ET_REQUEST_TO_JOIN_SERVER: {
        event_data_request_to_join_server_t *data = (event_data_request_to_join_server_t *)event->data;
        local_client_info_t client_info;
        client_info.name = local_client_info.client_name;

        if (data->server_name) {
            uint32_t *game_server_index = available_servers.name_to_server.get(simple_string_hash(data->server_name));
            if (game_server_index) {
                uint32_t ip_address = available_servers.servers[*game_server_index].ipv4_address;
            
                s_send_packet_connection_request(ip_address, &client_info);

                FL_FREE((void *)data->server_name);
            }
            else {
                LOG_ERRORV("Couldn't find server name: %s\n", data->server_name);

                FL_FREE((void *)data->server_name);
            }
        }
        else if (data->ip_address) {
            uint32_t ip_address = str_to_ipv4_int32(data->ip_address, GAME_OUTPUT_PORT_SERVER, SP_UDP);
            s_send_packet_connection_request(ip_address, &client_info);
        }
        else {
            
        }

        FL_FREE(data);
    } break;

    case ET_LEAVE_SERVER: {
        if (bound_server_address.ipv4_address > 0) {
            // Send to server message
            s_send_packet_client_disconnect();
        
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
    subscribe_to_event(ET_REQUEST_REFRESH_SERVER_PAGE, net_listener_id, events);

    socket_api_init();

    message_buffer = FL_MALLOC(char, MAX_MESSAGE_SIZE);

    s_hub_socket_init();
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
