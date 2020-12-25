#pragma once

#include <stdint.h>
#include <common/math.hpp>
#include <common/containers.hpp>

#include "vkph_weapon.hpp"
#include "vkph_constant.hpp"
#include "vkph_terraform.hpp"
#include "vkph_player_action.hpp"
#include "vkph_player_snapshot.hpp"

namespace vkph {

/* 
  Basically dictates how input affects player;
  - In PIM_METEORITE, the player just moves the mouse, and accelerates in a direction
  - In PIM_STANDING, the player is a walking humanoid who can use weapons and terraform 
    (gravity doesn't change)
  - In PIM_BALL, the player is a ball rolling around and the gravity changes as the 
    player rolls on surfaces with different normals - cannot use any weapons / terraform
*/
enum player_interaction_mode_t {
    PIM_NONE,
    PIM_METEORITE,
    PIM_STANDING,
    // For spectator
    PIM_FLOATING,
    PIM_BALL
};

union player_flags_t {
    struct {
        uint32_t is_remote: 1;
        uint32_t is_local: 1;
        uint32_t just_spawned: 1;
        uint32_t is_alive: 1;
        uint32_t interaction_mode: 3;
        uint32_t is_third_person: 1;
        uint32_t is_on_ground: 1;
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

struct player_init_info_t {
    // A bunch of shit will come

    const char *client_name;
    uint16_t client_id;
    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    float default_speed;
    uint32_t flags;
    vector3_t next_random_spawn_position;
};

struct player_t {

    /*
      An important bitfield that holds a lot of essential information about the player.
     */
    player_flags_t flags;

    /*
      This will correspond to the name of the client that was bound to this player.
      It is the same in every game and simply depends on the account that the user is using.
     */
    const char *name;

    /*
      The client ID simply refers to the an ID that was given by the server, corresponding
      to a unique number that only refers to this user.
      The local ID refers the the index of this player_t structure in vkph::state_t::players
      array.
     */
    uint16_t client_id;
    uint32_t local_id;

    vector3_t ws_position;
    vector3_t ws_view_direction;
    vector3_t ws_up_vector;
    vector3_t ws_velocity;
    vector3_t ws_surface_normal;
    float default_speed;
    float ball_speed;

    player_animated_state_t animated_state;

    /* 
      This *will* contain everything that has to do with rendering the player.
      Because the vkph module is common both to the client and server,
      this module won't contain ANY rendering code, hence the forward declaration.
     */
    struct player_render_t *render;

    uint32_t player_action_count;
    player_action_t player_actions[PLAYER_MAX_ACTIONS_COUNT];

    /*
      The player actions of each frame are cached in this structure until the client program
      send the client_commands_t packet to the server.
     */
    uint32_t cached_player_action_count;
    player_action_t *cached_player_actions;

    /*
      This will only be relevant to interpolating between the different states
      of remote players (not of the player that is currently playing).
    */
    circular_buffer_array_t<player_snapshot_t, 30> remote_snapshots;
    uint32_t snapshot_before, snapshot_after;
    float elapsed;

    /*
      If the client as an extremely high framerate, it is possible that when terraforming,
      nothing happens. This is because the app::g_delta_time is so small, that when casted
      to an int, the increase in voxel values doesn't even reach one. It is therefore necessary
      to accumulate the 'app::g_delta_time's until the increase in voxel values reaches >= 1.
    */
    float accumulated_dt;

    /* 
      Server will send this to the clients so that when clients spawn there is no delay
      Everytime a player gets added, a random position will be generated
    */
    vector3_t next_random_spawn_position;

    /*
      Necessary for third person camera transitision.
    */
    smooth_exponential_interpolation_t camera_distance;
    smooth_exponential_interpolation_t camera_fov;

    /*
      This is necessary for interpolating between different up vectors when the rolling player
      is changing surface normals.
     */
    vector3_t current_camera_up;
    vector3_t next_camera_up;

    terraform_info_t terraform_package;

    /* 
      Basically a timer (death checking for falling down forever).
    */
    float death_checker;

    /* 
       Switch shapes animation NOTE: This needs to be in the player_render_t struct.
    */
    bool switching_shapes;
    float shape_switching_time;

    /*
      This will control the rotation speed of a rolling player.
     */
    float frame_displacement;

    uint32_t weapon_count;
    uint32_t selected_weapon;
    weapon_t weapons[3];

    /* 
      Current chunk in which the player is in
    */
    ivector3_t chunk_coord;

    /* 
      Index of the player in chunk_t::players_in_chunk
      If idx_in_chunk_list == -1, the player hasn't been added to a chunk
    */
    int32_t idx_in_chunk_list;

    uint32_t health;



    void init(player_init_info_t *info, int16_t *client_to_local_id_map);
    void push_actions(player_action_t *action, bool override_adt);

    /*
      This function requires passing state_t in as it depends on the entire
      state of the game (terrain, other players, etc...).
    */
    void execute_action(player_action_t *action, state_t *state);

    /*
      View position here corresponds to the eye position -> need to add player height.
     */
    vector3_t compute_view_position();

private:

    void init_default_values(player_init_info_t *info);
    void init_weapons();

    /*
      Called in execute_action().
    */
    void handle_shape_switch(bool switch_shapes, float dt);
    void handle_weapon_switch(player_action_t *action);
    void execute_player_triggers(player_action_t *action, state_t *state);
    void accelerate_meteorite_player(player_action_t *action);
    void execute_player_direction_change(player_action_t *action);
    void execute_player_floating_movement(player_action_t *action);
    void execute_standing_player_movement(player_action_t *action);
    void execute_ball_player_movement(player_action_t *action);
    void check_player_dead(player_action_t *action, state_t *state);
    void update_player_chunk_status(state_t *state);

};

}
