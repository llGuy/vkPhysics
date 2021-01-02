#pragma once

#include <math.hpp>
#include "net_socket.hpp"
#include "net_packets.hpp"
#include "net_chunk_tracker.hpp"
#include "net_client_prediction.hpp"

#include <vkph_player.hpp>
#include <vkph_projectile.hpp>

namespace net {

constexpr uint32_t MAX_PREDICTED_PROJECTILE_HITS = 15;

/*
  Every client that will be connected to a game server will
  be represented with this structure.
  These will be not only created on the server to represent
  the currently connected clients, but also created on the client
  to represent remote clients.
  Of course however, not every field will be used in the client code
  just like not every field will be used in the server code.
*/
struct client_t {
    union {
        struct {
            uint32_t initialised: 1;
            uint32_t waiting_on_correction: 1;
            uint32_t received_first_commands_packet: 1;
            uint32_t chunks_to_wait_for: 15;
            /* 
              The `tick` variable should just be set after a game state snapshot dispatching happened
              because server only checks for prediction errors at game state dispatching.
              When an error occurs, server needs to be sure that `tick` is the last moment
              where client didn't make any errors.
            */
            uint32_t should_set_tick: 1;
            uint32_t did_terrain_mod_previous_tick: 1;
            uint32_t send_corrected_predicted_voxels: 1;

            // For the server: if the server receives the ping response: flip this bit
            uint32_t received_ping: 1;
            // Will use other bits in future
        };

        uint32_t flags = 0;
    };

    const char *name;
    uint16_t client_id;
    address_t address;
    // Used to make sure that the server should listen to a client.
    uint32_t client_tag;

    client_prediction_t predicted;

    // TODO: Get rid of this - it's redundant.
    uint32_t proj_hit_count;
    vkph::predicted_projectile_hit_t proj_hits[MAX_PREDICTED_PROJECTILE_HITS];

    /* 
      Store the previous locations so that we can do lag compensation.
    */
    circular_buffer_array_t<vkph::player_position_snapshot_t, 40> previous_locations;

    uint64_t tick;
    uint64_t tick_at_which_client_terraformed;

    /*
      When sending the world data to new clients, we need to make sure that
      we don't send ALL the chunk data at once because this would completely go
      beyond the memory boundaries of what is available in the internal socket
      receive buffer. We therefore need to carefully send the packets at 
      fixed time intervals. We will need to keep track of the remaining chunk
      packets to send over time.
    */
    uint32_t chunk_packet_count;
    uint32_t current_chunk_sending;
    packet_chunk_voxels_t chunk_packets[20];

    // The amount of time it takes for the client to receive a message from the server (vice versa)
    float ping;
    float ping_in_progress;
    float time_since_ping;
};

}
