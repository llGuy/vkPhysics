#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>
#include <common/containers.hpp>

void world_init(
    event_submissions_t *events);

void destory_world();

void handle_world_input();

void tick_world(
    event_submissions_t *events);

void gpu_sync_world(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer);

eye_3d_info_t create_eye_info();
lighting_info_t create_lighting_info();

typedef mesh_render_data_t player_render_data_t;

struct player_render_t {
    player_render_data_t render_data;
};

// There can be multiple of these (can be sent over network)
struct player_actions_t {
    union {
        struct {
            uint16_t move_forward: 1;
            uint16_t move_left: 1;
            uint16_t move_back: 1;
            uint16_t move_right: 1;
            uint16_t jump: 1;
            uint16_t crouch: 1;
            uint16_t trigger_left: 1;
            uint16_t trigger_right: 1;
        };

        uint16_t bytes;
    };
    
    float dmouse_x;
    float dmouse_y;
    float dt;

    // This is for terraforming
    float accumulated_dt;

    // For debugging
#if 1
    uint64_t tick;
#endif
};

#define MAX_PLAYER_ACTIONS 100

struct player_snapshot_t {
    union {
        struct {
            // This means that the server has detected that the client has predicted wrong information
            uint8_t client_needs_to_correct_state: 1;
            // This means that the server has still not received any correction acknowledgement, so don't send another correction
            // packet, because otherwise, the client will just go into some frenzy of constantly correcting
            uint8_t server_waiting_for_correction: 1;
            // Server sends this to client so that client can be sure to pop modification from accumulated history
            uint8_t terraformed: 1;
            // Will use in future
            uint8_t b3: 1;
            uint8_t b4: 1;
            uint8_t b5: 1;
            uint8_t b6: 1;
            uint8_t b7: 1;
        };
        uint8_t flags;
    };
    
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;

    // Tick that client has to revert to if client needs to do a correction
    uint64_t tick;
    // Tick that client has to clear chunks (not for correction, just generally)
    uint64_t terraform_tick;
};

// To initialise player, need to fill everything (except for player_render_t *render)
struct player_t {
    union {
        struct {
            uint32_t is_remote: 1;
            uint32_t is_local: 1;
        };

        uint32_t flags;
    };

    // Character name of player
    const char *name;
    // When accessing client information which holds stuff like IP address, etc...
    uint16_t client_id;
    // When accessing local player information
    uint32_t local_id;

    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;

    player_render_t *render;

    // Maximum player actions
    uint32_t player_action_count;
    player_actions_t player_actions[MAX_PLAYER_ACTIONS];

    // Only allocated for local player
    uint32_t cached_player_action_count;
    player_actions_t *cached_player_actions;

    // Will use this to interpolate between snapshots
    circular_buffer_array_t<player_snapshot_t, 30> remote_snapshots;
    float elapsed;

    float accumulated_dt;
};

player_t *get_player(
    uint16_t client_id);

// Just calls w_push_player_actions()
void push_player_actions(
    player_t *player,
    player_actions_t *actions);

// Push constant
// chunk_render_data_t may become a different structure in the future.
typedef mesh_render_data_t chunk_render_data_t;

struct chunk_render_t {
    mesh_t mesh;
    chunk_render_data_t render_data;
};

#define CHUNK_EDGE_LENGTH 16
#define CHUNK_VOXEL_COUNT CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH

#define MAX_VOXEL_VALUE_I 254
#define SPECIAL_VALUE 255

struct chunk_history_t {
    // These are all going to be set to 255 by default. If a voxel gets modified, modification_pool[voxel_index]
    // will be set to the initial value of that voxel before modifications
    uint8_t modification_pool[CHUNK_VOXEL_COUNT];

    int16_t modification_count;
    // Each int16_t is an index into the voxels array of struct chunk_t
    int16_t modification_stack[CHUNK_VOXEL_COUNT / 2];
};

struct chunk_t {
    struct flags_t {
        uint32_t made_modification: 1;
        uint32_t has_to_update_vertices: 1;
        uint32_t active_vertices: 1;
        // Flag that is used temporarily
        uint32_t modified_marker: 1;
        uint32_t index_of_modification_struct: 10;
    } flags;
    
    uint32_t chunk_stack_index;
    ivector3_t xs_bottom_corner;
    ivector3_t chunk_coord;

    uint8_t voxels[CHUNK_VOXEL_COUNT];

    //chunk_history_t *history;
    chunk_history_t history;

    chunk_render_t *render;
};

chunk_t *get_chunk(
    ivector3_t coord);

chunk_t **get_active_chunks(
    uint32_t *count);

void activate_chunk_history(
    chunk_t *chunk);

chunk_t **get_modified_chunks(
    uint32_t *count);

void reset_modification_tracker();

uint32_t get_voxel_index(
    uint32_t x,
    uint32_t y,
    uint32_t z);

void set_chunk_history_tracker_value(
    bool value);

struct chunks_to_interpolate_t {
    uint32_t max_modified;
    uint32_t modification_count;
    struct chunk_modifications_t *modifications;
};

chunks_to_interpolate_t *get_chunks_to_interpolate();

// For debugging only
stack_container_t<player_t *> &DEBUG_get_players();
