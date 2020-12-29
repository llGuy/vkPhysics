#pragma once

#include "vkph_voxel.hpp"
#include "vkph_terraform.hpp"

#include <math.hpp>

namespace vkph {

struct state_t;

/*
  Basically defines the plane in 3-dimensional space, of the player movement.
 */
struct movement_axes_t {
    vector3_t right;
    vector3_t up;
    vector3_t forward;
};

movement_axes_t compute_movement_axes(const vector3_t &view_direction, const vector3_t &up);

/*
  These are needed in order to compute the physics in moving players.
 */
enum movement_resolution_flags_t {
    MRF_ADOPT_GRAVITY = 1 << 0,
    MRF_GRAVITY_CHECK_INCLINATION = 1 << 1,
    MRF_ABRUPT_STOP = 1 << 2,
    MRF_CAP_SPEED = 1 << 3,
};

struct force_values_t {
    // This will be very high for standing mode
    float friction;
    // This will be very high for standing mode too (maybe?)
    float movement_acceleration;
    float gravity;
    float maximum_walking_speed;
};

enum movement_change_type_t {
    MCT_STOP, MCT_TOO_FAST, MCT_MADE_MOVEMENT
};

/*
  Some physics-math functions that are used when computing new player state.
 */
bool compute_acceleration_vector(
    struct player_t *player,
    struct player_action_t *action,
    movement_resolution_flags_t flags,
    force_values_t *force_values,
    vector3_t *dst);

void compute_velocity_vector(
    player_t *player,
    player_action_t *actions,
    force_values_t *force_values,
    movement_resolution_flags_t flags,
    const vector3_t &acceleration);

void resolve_player_movement(
    player_t *player,
    player_action_t *actions,
    force_values_t *force_values,
    int32_t flags,
    const state_t *state);

/*
  When doing any sort of collision with terrain (whether it be player-terrain,
  bullet-terrain, or ray-terrain), we will use this struct to retrieve information
  about that collision.

  NOTE: es = ellipsoid space
 */
struct terrain_collision_t {
    // Flags
    union {
        struct {
            uint32_t detected: 1;
            uint32_t has_detected_previously: 1;
            uint32_t under_terrain: 1;
            uint32_t recurse: 4;
        };
        uint32_t flags;
    };

    // Data passed between recursive calls
    // All these have to be filled in when calling collide_and_slide
    vector3_t es_position;
    vector3_t es_velocity;

    vector3_t ws_size;
    vector3_t ws_position;
    vector3_t ws_velocity;
    // -------------------------------------------------------------
    
    vector3_t es_normalised_velocity;
    vector3_t es_surface_normal;

    float es_nearest_distance;
    vector3_t es_contact_point;
};

/*
  Performs the collide and slide algorithm on an entity with the terrain.
  (It generates the chunk terrain mesh on the fly).
*/
vector3_t collide_and_slide(terrain_collision_t *collision, const state_t *state);

void check_ray_terrain_collision(terrain_collision_t *collision, const state_t *state);

terraform_package_t cast_terrain_ray(
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    voxel_color_t color,
    const state_t *state);

/*
  Collision detection with other types of bodies which are not the terrain.
*/
bool collide_sphere_with_standing_player(
    const vector3_t &player_pos,
    const vector3_t &player_up,
    const vector3_t &scenter,
    float sradius);

bool collide_sphere_with_rolling_player(
    const vector3_t &target_pos,
    const vector3_t &scenter,
    float sradius);

bool collide_sphere_with_player(
    const player_t *player,
    const vector3_t &scenter,
    float sradius);

/*
  These functions use the above functions.
 */
bool check_projectile_players_collision(struct rock_t *rock, int32_t *dst_player, const state_t *state);
bool check_projectile_terrain_collision(struct rock_t *rock, const state_t *state);

}
