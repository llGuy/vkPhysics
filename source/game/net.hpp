#pragma once

#include <common/tools.hpp>
#include <common/event.hpp>

void net_init(
    event_submissions_t *events);

struct network_address_t {
    uint16_t port;
    uint32_t ipv4_address;
};

struct client_t {
    union {
        struct {
            uint32_t initialised: 1;
            uint32_t waiting_on_correction: 1;
            //uint32_t server_received_correction: 1;
            uint32_t waiting_for_server_to_receive_correction: 1;
            // Will use other bits in future
        };

        uint32_t flags = 0;
    };

    const char *name;
    uint16_t client_id;
    network_address_t address;

    vector3_t ws_predicted_position;
    vector3_t ws_predicted_view_direction;
    vector3_t ws_predicted_up_vector;
};

struct local_client_info_t {
    const char *name;
};

void tick_net(
    event_submissions_t *events);

bool connected_to_server();
