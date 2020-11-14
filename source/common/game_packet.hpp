#pragma once

#include "net.hpp"
#include "team.hpp"
#include "game.hpp"

// PACKET TYPES ///////////////////////////////////////////////////////////////
enum packet_type_t {
    // Client sends to server when requesting to join game
    PT_CONNECTION_REQUEST,
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



// HEADER /////////////////////////////////////////////////////////////////////
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

uint32_t packed_packet_header_size();

void serialise_packet_header(packet_header_t *header, serialiser_t *serialiser);
void deserialise_packet_header(packet_header_t *header, serialiser_t *serialiser);



// DEFINITION OF PACKET TYPES /////////////////////////////////////////////////
#define CLIENT_NAME_MAX_LENGTH 50

struct packet_connection_request_t {
    const char *name;
};

uint32_t packed_connection_request_size(packet_connection_request_t *connection_request);
void deserialise_connection_request(packet_connection_request_t *request, serialiser_t *serialiser);
void serialise_connection_request(packet_connection_request_t *packet, serialiser_t *serialiser);

struct full_player_info_t {
    const char *name;
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_next_random_position;

    float default_speed;

    player_flags_t flags;
};

// Will use this when new player joins
struct packet_connection_handshake_t {
    // Chunks will be sent in separate packets (too much data)
    // These are the number of chunks that are incoming
    uint32_t loaded_chunk_count;
    // TODO: In future, find a way to only have chunks nearby to be sent to the client possibly?

    uint32_t player_count;
    full_player_info_t *player_infos;

    uint32_t team_count;
    team_info_t *team_infos;
};

uint32_t packed_connection_handshake_size(packet_connection_handshake_t *game_state);
void serialise_connection_handshake(packet_connection_handshake_t *full_game_state, serialiser_t *serialiser);
void deserialise_connection_handshake(packet_connection_handshake_t *full_game_state, serialiser_t *serialiser);

// Will be sent from server to all players
struct packet_player_joined_t {
    full_player_info_t player_info;
};

uint32_t packed_player_joined_size(packet_player_joined_t *packet);
void serialise_player_joined(packet_player_joined_t *packet, serialiser_t *serialiser);
void deserialise_player_joined(packet_player_joined_t *packet, serialiser_t *serialiser);

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
    player_action_t *actions;

    // Stuff that the server will use to compare server-calculated data
    uint32_t player_flags;

    vector3_t ws_final_position;
    vector3_t ws_final_view_direction;
    vector3_t ws_final_up_vector;

    vector3_t ws_final_velocity;
    
    uint32_t modified_chunk_count;
    chunk_modifications_t *chunk_modifications;
};

uint32_t packed_player_commands_size(packet_client_commands_t *commands);
void serialise_player_commands(packet_client_commands_t *packet, serialiser_t *serialiser);
void deserialise_player_commands(packet_client_commands_t *packet, serialiser_t *serialiser);

// Will use this during game play
struct packet_game_state_snapshot_t {
    uint32_t player_data_count;
    player_snapshot_t *player_snapshots;

    // Total chunk modifications that occured in the entire world
    uint32_t modified_chunk_count;
    chunk_modifications_t *chunk_modifications;
};

uint32_t packed_game_state_snapshot_size(packet_game_state_snapshot_t *packet);
void serialise_game_state_snapshot(packet_game_state_snapshot_t *packet, serialiser_t *serialiser);
void deserialise_game_state_snapshot(packet_game_state_snapshot_t *packet, serialiser_t *serialiser);

enum color_serialisation_type_t { CST_SERIALISE_UNION_COLOR = 0, CST_SERIALISE_SEPARATE_COLOR = 1 };

// color_serialisation_type_t parameter refers to whether to (de)serialise the color value from the colors array
// or to (de)serialise the color value from the union in the voxel_modification_t struct
void serialise_chunk_modifications(chunk_modifications_t *modifications, uint32_t modification_count, serialiser_t *serialiser, color_serialisation_type_t);
chunk_modifications_t *deserialise_chunk_modifications(uint32_t *modification_count, serialiser_t *serialiser, color_serialisation_type_t);

struct voxel_chunk_values_t {
    // Chunk coord
    int16_t x, y, z;
    voxel_t *voxel_values;
};

struct packet_chunk_voxels_t {
    uint32_t chunk_in_packet_count;
    voxel_chunk_values_t *values;
};

uint32_t packed_chunk_voxels_size(packet_chunk_voxels_t *packet);
void serialise_packet_chunk_voxels(packet_chunk_voxels_t *packet, serialiser_t *serialiser);
void deserialise_packet_chunk_voxels(packet_chunk_voxels_t *packet, serialiser_t *serialiser);

struct packet_player_team_change_t {
    uint16_t client_id;
    uint16_t color;
};

uint32_t packed_player_team_change_size();
void serialise_packet_player_team_change(packet_player_team_change_t *packet, serialiser_t *serialiser);
void deserialise_packet_player_team_change(packet_player_team_change_t *packet, serialiser_t *serialiser);
