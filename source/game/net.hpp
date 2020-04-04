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
    const char *name;
    uint16_t client_id;
    network_address_t address;
};

struct local_client_info_t {
    const char *name;
};

void tick_net(
    event_submissions_t *events);
