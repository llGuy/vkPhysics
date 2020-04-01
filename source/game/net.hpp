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

void start_client();

struct local_client_info_t {
    const char *name;
};

void send_connect_request_to_server(
    const char *ip_address,
    local_client_info_t *info);

// Start this on a separate thread (in case client wants to host a server)
// (In which case the client would simply connect to 127.0.0.1)
void start_server();

void tick_net(
    event_submissions_t *events);
