#pragma once

#include <stdint.h>
#include <math.hpp>

namespace vkph {

/*
  If a client has to revert in case of a prediction error, client will need to
  revert back to the tick that is declared in struct snapshot_t.
 */
struct player_snapshot_t {
    union {
        struct {
            // This means that the server has detected that the client has predicted wrong information
            uint16_t client_needs_to_correct_state: 1;
            // This means that the server has still not received any correction acknowledgement, so don't send another correction
            // packet, because otherwise, the client will just go into some frenzy of constantly correcting
            uint16_t server_waiting_for_correction: 1;
            // Server sends this to client so that client can be sure to pop modification from accumulated history
            uint16_t terraformed: 1;
            uint16_t packet_contains_terrain_correction: 1;
            uint16_t alive_state: 2;
            uint16_t interaction_mode: 3;
            uint16_t contact: 1;
            uint16_t animated_state: 4;
        };
        uint16_t flags;
    };
    
    uint16_t client_id;
    uint32_t player_local_flags;
    uint32_t player_health;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_next_random_spawn;
    vector3_t ws_velocity;
    float frame_displacement;

    uint64_t tick;
    // Tick that client has to clear chunks (not for correction, just generally)
    uint64_t terraform_tick;
};

struct player_position_snapshot_t {
    vector3_t ws_position;
    uint64_t tick;
};

}
