#pragma once

#include <common/event.hpp>
#include <renderer/renderer.hpp>

void world_init(
    event_submissions_t *events);

void destory_world();

void handle_world_input();

void tick_world(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    event_submissions_t *events);

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

#define MAX_PLAYER_ACTIONS 15

// To initialise player, need to fill everything (except for player_render_t *render)
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
    float default_speed;

    player_render_t *render;

    // Maximum player actions
    uint32_t player_action_count;
    player_actions_t player_actions[MAX_PLAYER_ACTIONS];
};

player_t *get_player(
    uint16_t client_id);
