#pragma once

#include <stdint.h>

struct local_client_info_t {
    const char *name;
};

void nw_init(struct event_submissions_t *events);
void nw_tick(struct event_submissions_t *events);
bool nw_connected_to_server();
uint16_t nw_get_local_client_index();
void nw_check_registration(event_submissions_t *events);
