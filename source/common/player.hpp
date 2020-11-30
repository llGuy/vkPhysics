#pragma once

#include "math.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "tools.hpp"
#include "weapon.hpp"
#include "constant.hpp"
#include "containers.hpp"

// There can be multiple of these (can be sent over network)
struct player_action_t {
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
            uint16_t switch_weapons: 1;
            // All 1s is an invalid weapon. The player is just cycling through weapons
            uint16_t next_weapon: 3;
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
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_next_random_spawn;
    vector3_t ws_velocity;
    float frame_displacement;

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

#define SHAPE_SWITCH_ANIMATION_TIME 0.3f

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
        // Contain the team color
        uint32_t team_color: 5;
    };

    uint32_t u32;
};

enum player_animated_state_t {
    PAS_IDLE,
    PAS_WALKING,
    PAS_RUNNING,
    PAS_JUMPING_UP,
    PAS_JUMPING_DOWN,
    PAS_LEFT_WALKING,
    PAS_RIGHT_WALKING,
    PAS_STOP_FAST,
    PAS_BACKWALKING,
    PAS_TRIPPING
};

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

    player_animated_state_t animated_state;

    // This will  contain everything that has to do with rendering the player
    struct player_render_t *render;

    // Maximum player actions
    uint32_t player_action_count;
    player_action_t player_actions[PLAYER_MAX_ACTIONS_COUNT];

    // Only allocated for local player
    uint32_t cached_player_action_count;
    player_action_t *cached_player_actions;

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

    terraform_package_t terraform_package;

    // Basically a timer (death checking for falling down forever)
    float death_checker;

    // Switch shapes animation
    // This needs to be in the player_render_t struct
    bool switching_shapes;
    float shape_switching_time;

    float frame_displacement;

    vector3_t jump_vector;

    uint32_t weapon_count;
    uint32_t selected_weapon;
    // 2 weapons maximum (add more in future if needed)
    weapon_t weapons[3];

    // Current chunk in which the player is in
    ivector3_t chunk_coord;
    // Index of the player in chunk_t::players_in_chunk
    // If idx_in_chunk_list == -1, the player hasn't been added to a chunk
    int32_t idx_in_chunk_list;

};

void fill_player_info(player_t *player, player_init_info_t *info);
void push_player_actions(player_t *player, player_action_t *action, bool override_adt);
// If false was returned, the player died
void execute_action(player_t *player, player_action_t *action);
void handle_shape_switch(player_t *p, bool switch_shapes, float dt);
vector3_t compute_player_view_position(const player_t *p);

bool collide_sphere_with_player(const player_t *p, const vector3_t &center, float radius);

// Make sure chunk has correct information as to where each player is
void update_player_chunk_status(player_t *p);
