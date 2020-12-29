#pragma once

#include <stdint.h>
#include <containers.hpp>

namespace net {

/*
  Just some stuff with dealing with meta servers.
*/
struct game_server_t {
    const char *server_name;
    uint32_t ipv4_address;
};

struct available_servers_t {
    uint32_t server_count;
    game_server_t *servers;

    hash_table_t<uint32_t, 50, 5, 5> name_to_server;
};

}
