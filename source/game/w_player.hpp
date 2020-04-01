#pragma once

#include <common/tools.hpp>

struct player_t {
    // Character name of player
    const char *name;
    // When accessing client information which holds stuff like IP address, etc...
    uint16_t client_id;
    // When accessing local player information
    uint32_t local_id;

    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
};
