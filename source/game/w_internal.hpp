#pragma once

#include "world.hpp"
#include <common/tools.hpp>
#include <renderer/input.hpp>
#include <common/containers.hpp>
#include <renderer/renderer.hpp>

#define MAX_VOXEL_VALUE_F 254.0f
#define MAX_VERTICES_PER_CHUNK 5 * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1)

void w_push_player_actions(
    player_t *player,
    player_actions_t *action,
    bool override_adt);

void w_players_data_init();

void w_player_render_init(
    player_t *player);

player_t *w_add_player(
    struct world_t *world);

void w_handle_input(
    game_input_t *input,
    float dt,
    struct world_t *world);

void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    struct world_t *world);

player_t *w_get_local_player(
    struct world_t *world);

player_t *w_get_player_from_client_id(
    uint16_t client_id,
    struct world_t *world);

void w_link_client_id_to_local_id(
    uint16_t client_id,
    uint32_t local_id,
    struct world_t *world);

void w_player_world_init(
    struct world_t *world);

void w_tick_players(
    struct world_t *world);

void w_set_local_player(
    int32_t local_id,
    struct world_t *world);

player_t *w_get_spectator(
    struct world_t *world);

void w_destroy_player(
    uint32_t id,
    world_t *world);

void w_clear_players(
    world_t *world);

uint32_t w_get_voxel_index(
    uint32_t x,
    uint32_t y,
    uint32_t z);

void w_chunk_init(
    chunk_t *chunk,
    uint32_t chunk_stack_index,
    const ivector3_t &chunk_coord);

void w_chunk_render_init(
    chunk_t *chunk,
    const vector3_t &ws_position);

// Would call this at end of game or when chunk is out of chunk load radius.
void w_destroy_chunk_render(
    chunk_t *chunk);

// Returns NULL
// Usage: chunk = w_destroy_chunk(chun)
chunk_t *w_destroy_chunk(
    chunk_t *chunk);

// Max loaded chunks for now (loaded chunk = chunk with active voxels)
#define MAX_LOADED_CHUNKS 1000
#define MAX_PLAYERS 50

struct world_t {
    int32_t local_player;
    player_t *spectator;
    stack_container_t<player_t *> players;
    // From client id, get player
    int16_t local_id_from_client_id[MAX_PLAYERS];

    uint32_t loaded_radius;
    // List of chunks
    // Works like a stack
    stack_container_t<chunk_t *> chunks;
    uint32_t render_count;
    chunk_t **chunks_to_render;
    hash_table_t<uint32_t, 200, 30, 5> chunk_indices;

    uint32_t max_modified_chunks;
    uint32_t modified_chunk_count;
    chunk_t **modified_chunks;

    struct {
        uint8_t wait_mesh_update: 1;
        uint8_t track_history: 1;
        uint8_t in_server: 1;
    };
};

uint32_t w_hash_chunk_coord(
    const ivector3_t &coord);

void w_chunks_data_init();

void w_clear_chunk_world(
    world_t *world);

void w_destroy_chunk_data();

void w_chunk_world_init(
    world_t *world,
    uint32_t loaded_radius);

void w_tick_chunks(
    float dt);

void w_chunk_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    world_t *world);

// Any function suffixed with _m means that the function will cause chunks to be added to a list needing gpusync
void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius,
    world_t *world);

ivector3_t w_convert_world_to_voxel(
    const vector3_t &ws_position);

ivector3_t w_convert_voxel_to_chunk(
    const ivector3_t &vs_position);

vector3_t w_convert_chunk_to_world(
    const ivector3_t &chunk_coord);

ivector3_t w_convert_voxel_to_local_chunk(
    const ivector3_t &vs_position);

// Does not create a chunk if it wasn't already created
chunk_t *w_access_chunk(
    const ivector3_t &coord,
    world_t *world);

// If chunk was not created, create it
chunk_t *w_get_chunk(
    const ivector3_t &coord,
    world_t *world);

enum terraform_type_t { TT_DESTROY, TT_BUILD };

void w_terraform(
    terraform_type_t type,
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    float radius,
    float speed,
    float dt,
    world_t *world);

void w_toggle_mesh_update_wait(
    bool value,
    world_t *world);

uint8_t get_surface_level();

// enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };

// struct terrain_collision_t {
//     union {
//         struct {
//             uint32_t detected: 1;
//             uint32_t under_terrain: 1;
//             uint32_t is_currently_in_air: 1;
//         };
//         uint32_t flags;
//     };

//     // Velocity after sliding on terrain
//     vector3_t es_new_velocity;

//     // Point of contact between bounding sphere and terrain
//     vector3_t es_contact_point;

//     // Center position of sphere when collision happened
//     vector3_t es_collision_point;

//     // Normal of terrain at contact point
//     vector3_t es_surface_normal;

//     // Distance between contact point and center
//     float es_distance_to_center;

//     float es_distance_from_triangle;

//     collision_primitive_type_t primitive_type;
// };

// terrain_collision_t collide_and_slide(
//     const vector3_t &es_center,
//     const vector3_t &es_velocity,
//     const vector3_t &ws_size,
//     uint32_t recurse_depth,
//     terrain_collision_t previous_collision,
//     terrain_collision_t *first_collision);

// TODO: Make sure to revise collision detection implementation!

enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };

struct terrain_collision_t {
    // Flags
    union {
        struct {
            uint32_t detected: 1;
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

    float es_nearest_distance;
    vector3_t es_contact_point;
};

struct collision_triangle_t {
    union {
        struct {
            vector3_t a;
            vector3_t b;
            vector3_t c;
        } v;
        vector3_t vertices[3];
    };
};

vector3_t collide_and_slide(
    const vector3_t &ws_size,
    const vector3_t &ws_position,
    const vector3_t &ws_velocity,
    terrain_collision_t *previous);
