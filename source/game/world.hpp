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
};

#define MAX_PLAYER_ACTIONS 100

struct player_snapshot_t {
    union {
        struct {
            uint8_t client_needs_to_correct: 1;
            uint8_t b1: 1;
            // Will use in future
            uint8_t b2: 1;
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
};

player_t *get_player(
    uint16_t client_id);

// Just calls w_push_player_actions()
void push_player_actions(
    player_t *player,
    player_actions_t *actions);

// For debugging only
stack_container_t<player_t *> &DEBUG_get_players();
