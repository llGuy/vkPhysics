#pragma once

#include "net.hpp"
#include "world.hpp"
#include <common/tools.hpp>
#include <common/serialiser.hpp>

enum packet_type_t {
    // Client sends to server when requesting to join game
    PT_CONNECTION_REQUEST,
    // Server sends to client when join request was acknowledged and client can join
    PT_CONNECTION_HANDSHAKE,
    // Server sends to clients when a new player joins
    PT_PLAYER_JOINED,
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
};

uint32_t n_packed_packet_header_size();

void n_serialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser);

void n_deserialise_packet_header(
    packet_header_t *header,
    serialiser_t *serialiser);

#define CLIENT_NAME_MAX_LENGTH 50

struct packet_connection_request_t {
    const char *name;
};

uint32_t n_packed_connection_request_size(
    packet_connection_request_t *connection_request);

void n_deserialise_connection_request(
    packet_connection_request_t *request,
    serialiser_t *serialiser);

void n_serialise_connection_request(
    packet_connection_request_t *packet,
    serialiser_t *serialiser);

struct full_player_info_t {
    const char *name;
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;

    union {
        struct {
            uint32_t is_remote: 1;
            uint32_t is_local: 1;
            uint32_t alive_state: 2;
        };

        uint32_t flags;
    };
};

// Will use this when new player joins
struct packet_connection_handshake_t {
    // Chunks will be sent in separate packets (too much data)
    // These are the number of chunks that are incoming
    uint32_t loaded_chunk_count;
    // TODO: In future, find a way to only have chunks nearby to be sent to the client possibly?

    uint32_t player_count;
    full_player_info_t *player_infos;
};

uint32_t n_packed_connection_handshake_size(
    packet_connection_handshake_t *game_state);

void n_serialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser);

void n_deserialise_connection_handshake(
    packet_connection_handshake_t *full_game_state,
    serialiser_t *serialiser);

// Will be sent from server to all players
struct packet_player_joined_t {
    full_player_info_t player_info;
};

uint32_t n_packed_player_joined_size(
    packet_player_joined_t *packet);

void n_serialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser);

void n_deserialise_player_joined(
    packet_player_joined_t *packet,
    serialiser_t *serialiser);

struct packet_player_commands_t {
    union {
        struct {
            uint8_t did_correction: 1;
        };

        uint8_t flags;
    };

    uint8_t command_count;
    player_actions_t *actions;

    // Stuff that the server will use to compare server-calculated data
    vector3_t ws_final_position;
    vector3_t ws_final_view_direction;
    vector3_t ws_final_up_vector;
    
    uint32_t modified_chunk_count;
    chunk_modifications_t *chunk_modifications;
};

uint32_t n_packed_player_commands_size(
    packet_player_commands_t *commands);

void n_serialise_player_commands(
    packet_player_commands_t *packet,
    serialiser_t *serialiser);

void n_deserialise_player_commands(
    packet_player_commands_t *packet,
    serialiser_t *serialiser);

// Will use this during game play
struct packet_game_state_snapshot_t {
    uint32_t player_data_count;
    player_snapshot_t *player_snapshots;

    // Total chunk modifications that occured in the entire world
    uint32_t modified_chunk_count;
    chunk_modifications_t *chunk_modifications;
};

uint32_t n_packed_game_state_snapshot_size(
    packet_game_state_snapshot_t *packet);

void n_serialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser);

void n_deserialise_game_state_snapshot(
    packet_game_state_snapshot_t *packet,
    serialiser_t *serialiser);

void n_serialise_chunk_modifications(
    chunk_modifications_t *modifications,
    uint32_t modification_count,
    serialiser_t *serialiser);

chunk_modifications_t *n_deserialise_chunk_modifications(
    uint32_t *modification_count,
    serialiser_t *serialiser);

// packet_chunk_voxels_t defined in net.hpp
uint32_t n_packed_chunk_voxels_size(
    packet_chunk_voxels_t *packet);

void n_serialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser);

void n_deserialise_packet_chunk_voxels(
    packet_chunk_voxels_t *packet,
    serialiser_t *serialiser);
