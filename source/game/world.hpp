#pragma once

#include "game/engine.hpp"
#include <common/math.hpp>
#include <common/time.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>
#include <common/containers.hpp>

#define PLAYER_WALKING_SPEED 25.0f

void world_init(
    event_submissions_t *events);

void destory_world();

void handle_world_input(
    highlevel_focus_t focus);

void tick_world(
    event_submissions_t *events);

void gpu_sync_world(
    bool in_startup,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
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
            // Switch between player mode and ball mode
            uint16_t switch_shapes: 1;
            uint16_t flashlight: 1;
            uint16_t dead: 1;
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
            uint16_t client_needs_to_correct_state: 1;
            // This means that the server has still not received any correction acknowledgement, so don't send another correction
            // packet, because otherwise, the client will just go into some frenzy of constantly correcting
            uint16_t server_waiting_for_correction: 1;
            // Server sends this to client so that client can be sure to pop modification from accumulated history
            uint16_t terraformed: 1;
            uint16_t packet_contains_terrain_correction: 1;
            uint16_t alive_state: 2;
            uint16_t interaction_mode: 3;
        };
        uint16_t flags;
    };
    
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_next_random_spawn;
    vector3_t ws_velocity;

    // Tick that client has to revert to if client needs to do a correction
    uint64_t tick;
    // Tick that client has to clear chunks (not for correction, just generally)
    uint64_t terraform_tick;
};

// To initialise player, need to fill everything (except for player_render_t *render)
enum player_alive_state_t {
    PAS_DEAD, PAS_ALIVE
};

// Basically dictates how input affects player
// In PIM_METEORITE, the player just moves the mouse, and accelerates in a direction
// In PIM_STANDING, the player is a walking humanoid who can use weapons and terraform (gravity doesn't change)
// In PIM_BALL, the player is a ball rolling around and the gravity changes as the player rolls on surfaces with different normals - cannot use any weapons / terraform
enum player_interaction_mode_t {
    PIM_NONE,
    PIM_METEORITE,
    PIM_STANDING,
    // For spectator
    PIM_FLOATING,
    PIM_BALL
};

enum player_contact_state_t {
    PCS_IN_AIR, PCS_ON_GROUND
};

enum camera_type_t {
    CT_FIRST_PERSON = 0, CT_THIRD_PERSON = 1
};

union player_flags_t {
    struct {
        uint32_t is_remote: 1;
        uint32_t is_local: 1;
        uint32_t just_spawned: 1;
        uint32_t alive_state: 2;
        uint32_t interaction_mode: 3;
        uint32_t camera_type: 1;
        uint32_t contact: 1;
        uint32_t moving: 1;
        uint32_t flashing_light: 1;
    };

    uint32_t u32;
};

struct terraform_package_t {
    vector3_t position;
    bool ray_hit_terrain;
};

enum player_animated_state_t {
    PAS_IDLE,
    PAS_WALKING,
    PAS_RUNNING,
    PAS_JUMPING_UP,
    PAS_JUMPING_DOWN
};

#define SHAPE_SWITCH_ANIMATION_TIME 0.3f

// TODO: Make sure to only allocate camera stuff for local player
struct player_t {
    player_flags_t flags;

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

    vector3_t ws_velocity;
    vector3_t ws_surface_normal;

    float ball_speed;

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

    // Server will send this to the clients so that when clients spawn there is no delay
    // Everytime a player gets added, a random position will be generated
    vector3_t next_random_spawn_position;

    // When in third person, distance of the camera to the player
    smooth_exponential_interpolation_t camera_distance;
    smooth_exponential_interpolation_t camera_fov;
    // This will be constantly interpolating depending on the surface the ball is on
    vector3_t current_camera_up;
    vector3_t next_camera_up;

    animated_instance_t animations;

    terraform_package_t terraform_package;

    struct {
        uint8_t animated_state: 7;
        uint8_t repeat: 1;
    };

    // Basically a timer (death checking for falling down forever)
    float death_checker;

    // For AI
    struct {
        uint8_t ai: 1;
        uint8_t calculated_score: 1;
    };
    uint32_t ai_id;
    float ai_start_time;

    // Switch shapes animation
    bool switching_shapes;
    float shape_animation_time;

    float frame_displacement;
    float rotation_speed;
    float rotation_angle;

    matrix4_t rolling_matrix;

    vector3_t jump_vector;
};

// Rewrite this crap
player_t *get_player_from_client_id(
    uint16_t client_id);

player_t *get_player_from_player_id(
    uint32_t player_id);

// TODO: Replace this with the a function taking the player id instead of a pointer
// Just calls w_push_player_actions()
void push_player_actions(
    player_t *player,
    player_actions_t *actions,
    bool override_adt);

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
    const ivector3_t &coord);

chunk_t *access_chunk(
    const ivector3_t &coord);

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
    float elapsed;
    uint32_t max_modified;
    uint32_t modification_count;
    struct chunk_modifications_t *modifications;
};

chunks_to_interpolate_t *get_chunks_to_interpolate();

// For AI
#define AI_SENSOR_COUNT 26
struct sensors_t {
    float *s;
};

void cast_ray_sensors(
    sensors_t *sensors,
    const vector3_t &ws_position,
    const vector3_t &ws_view_direction,
    const vector3_t &ws_up_vector);

// For debugging only
stack_container_t<player_t *> &DEBUG_get_players();
player_t *DEBUG_get_spectator();

void read_startup_screen();
void write_startup_screen();

void kill_local_player(
    event_submissions_t *events);
