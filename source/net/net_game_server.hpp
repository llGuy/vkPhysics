#pragma once

#include <stdint.h>
#include <containers.hpp>

#include "net_socket.hpp"

namespace net {

/*
  Just some stuff with dealing with meta servers.
*/
struct game_server_t {
    const char *server_name;
    const char *ip_addr_str;
    address_t ipv4_address;
    uint32_t tag;
};

struct available_servers_t {
    uint32_t server_count;
    game_server_t *servers;

    hash_table_t<uint32_t, 50, 5, 5> name_to_server;
};

}
