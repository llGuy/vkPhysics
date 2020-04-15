#pragma once

#include <common/tools.hpp>
#include <common/event.hpp>
#include <common/socket.hpp>

void net_init(
    event_submissions_t *events);

#define MAX_PREDICTED_CHUNK_MODIFICATIONS 20
#define MAX_PREDICTED_VOXEL_MODIFICATIONS_PER_CHUNK 250

struct voxel_modification_t {
    uint16_t index;
    uint8_t final_value;
    // This does not get sent over network
    uint8_t initial_value;
};

struct chunk_modifications_t {
    int16_t x, y, z;
    uint32_t modified_voxels_count;
    voxel_modification_t modifications[MAX_PREDICTED_VOXEL_MODIFICATIONS_PER_CHUNK];

    union {
        uint8_t flags;
        struct {
            uint8_t needs_to_correct: 1;
        };
    };
};

struct client_t {
    union {
        struct {
            uint32_t initialised: 1;
            uint32_t waiting_on_correction: 1;
            uint32_t received_first_commands_packet: 1;
            uint32_t chunks_to_wait_for: 15;
            // The `tick` variable should just be set after a game state snapshot dispatching happened
            // Because server only checks for prediction errors at game state dispatching
            // When an error occurs, server needs to be sure that `tick` is the last moment
            // Where client didn't make any errors
            uint32_t should_set_tick: 1;
            uint32_t did_terrain_mod_previous_tick: 1;
            // Will use other bits in future
        };

        uint32_t flags = 0;
    };

    const char *name;
    uint16_t client_id;
    network_address_t address;

    vector3_t ws_predicted_position;
    vector3_t ws_predicted_view_direction;
    vector3_t ws_predicted_up_vector;

    // Predicted chunk modifications
    uint32_t predicted_chunk_mod_count;
    chunk_modifications_t *predicted_modifications;

    uint64_t tick;
    uint64_t tick_at_which_client_terraformed;
};

struct local_client_info_t {
    const char *name;
};

void tick_net(
    event_submissions_t *events);

bool connected_to_server();
float server_snapshot_interval();
float client_command_interval();

struct voxel_chunk_values_t {
    // Chunk coord
    int16_t x, y, z;
    uint8_t *voxel_values;
};

struct packet_chunk_voxels_t {
    uint32_t chunk_in_packet_count;
    voxel_chunk_values_t *values;
};
