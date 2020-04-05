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
            uint8_t initialised: 1;
            // Will use in future
            uint8_t b1: 1;
            uint8_t b2: 1;
            uint8_t b3: 1;
            uint8_t b4: 1;
            uint8_t b5: 1;
            uint8_t b6: 1;
            uint8_t b7: 1;
        };

        uint8_t flags = 0;
    };

    const char *name;
    uint16_t client_id;
    network_address_t address;
};

struct local_client_info_t {
    const char *name;
};

void tick_net(
    event_submissions_t *events);

bool connected_to_server();
