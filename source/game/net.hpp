#pragma once

#include <common/tools.hpp>
#include <common/event.hpp>

void net_init();

void start_client();

void tick_client(
    event_submissions_t *events);

struct local_client_info_t {
    const char *name;
};

void send_connect_request_to_server(
    const char *ip_address,
    local_client_info_t *info);

void start_server();
