#pragma once

#include <stdint.h>
#include <vkph_team.hpp>
#include <vkph_player.hpp>
#include <vkph_projectile.hpp>
#include <common/serialiser.hpp>

#include "net_chunk_tracker.hpp"
#include "net_client_prediction.hpp"

namespace net {

// PACKET TYPES ///////////////////////////////////////////////////////////////
enum packet_type_t {
    // Client sends to server when requesting to join game
    PT_CONNECTION_REQUEST,
    // Packet sent from both server and client
    PT_PING,
    // Server sends to client when join request was acknowledged and client can join
    PT_CONNECTION_HANDSHAKE,
    // Server sends to clients when a new player joins
    PT_PLAYER_JOINED,
    // When a client clicks on a team to join
    PT_TEAM_SELECT_REQUEST,
    // When a player has joined / changed teams
    PT_PLAYER_TEAM_CHANGE,
    // Client sends to server when wishing to disconnect
    PT_CLIENT_DISCONNECT,
    // Server sends to clients when player has disconnected
    PT_PLAYER_LEFT,
    // Client sends to server at short time intervals with player actions (movement, looking, etc...)
    PT_CLIENT_COMMANDS,
    // Server sends this to clients at intervals with current game state
    PT_GAME_STATE_SNAPSHOT,
    // Server sends this to the clients when they join at the beginning
    PT_CHUNK_VOXELS,
};

/*
  Header of all game packets sent over the network.

  NOTE: Creating a base class and overriding the functions size, 
  serialise and deserialise is not necessary - no polymorphism
  is being used with these structures.
 */
struct packet_header_t {
    union {
        struct {
            uint32_t packet_type: 10;
            // Includes header
            uint32_t total_packet_size: 22;
        };

        uint32_t bytes;
    } flags;

    uint64_t current_tick;
    uint64_t current_packet_count;
    uint32_t client_id;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

struct packet_connection_request_t {
    const char *name;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

struct packet_connection_handshake_t {
    // Chunks will be sent in separate packets (too much data)
    // These are the number of chunks that are incoming
    uint32_t loaded_chunk_count;

    uint32_t player_count;
    vkph::player_init_info_t *player_infos;

    uint32_t team_count;
    vkph::team_info_t *team_infos;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

struct packet_player_joined_t {
    vkph::player_init_info_t player_info;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

struct packet_client_commands_t {
    union {
        struct {
            uint8_t did_correction: 1;
            // This will spawn on the client computer, the server will then also spawn immediately
            uint8_t requested_spawn: 1;
        };

        uint8_t flags;
    };

    uint8_t command_count;
    vkph::player_action_t *actions;
    client_prediction_t prediction;

    // TODO: Remove
    uint32_t predicted_hit_count;
    vkph::predicted_projectile_hit_t *hits;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

// Will use this during game play
struct packet_game_state_snapshot_t {
    uint32_t player_data_count;
    vkph::player_snapshot_t *player_snapshots;

    // All the projectiles that have been spawned
    uint32_t rock_count;
    vkph::rock_snapshot_t *rock_snapshots;

    // Total chunk modifications that occured in the entire world
    uint32_t modified_chunk_count;
    chunk_modifications_t *chunk_modifications;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

struct voxel_chunk_values_t {
    // Chunk coord
    int16_t x, y, z;
    const vkph::voxel_t *voxel_values;
};

struct packet_chunk_voxels_t {
    vkph::voxel_t *chunk_data;
    uint32_t size;

    /* 
      NOTE: No need to write serialise / deserialise functions because it will
      directly written by client / server code (need extra control).
    */
};

struct packet_player_team_change_t {
    uint16_t client_id;
    uint16_t color;

    uint32_t size();
    void serialise(serialiser_t *serialiser);
    void deserialise(serialiser_t *serialiser);
};

}
