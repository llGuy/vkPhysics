#pragma once

#include "tools.hpp"
#include "constant.hpp"

void chunk_memory_init();
// If on client, client will have to take care of clearing the rendering resources
void clear_chunks();

ivector3_t space_world_to_voxel(const vector3_t &ws_position);
ivector3_t space_voxel_to_chunk(const ivector3_t &vs_position);
vector3_t space_chunk_to_world(const ivector3_t &chunk_coord);
ivector3_t space_voxel_to_local_chunk(const ivector3_t &vs_position);
uint32_t get_voxel_index(uint32_t x, uint32_t y, uint32_t z);

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

    struct chunk_render_t *render;
};

void chunk_init(chunk_t *chunk, uint32_t chunk_stack_index, const ivector3_t &chunk_coord);
// If on client side, client will have to handle destroying the rendering resources of the chunk
void destroy_chunk(chunk_t *chunk);
// When this is called, if the chunk wasn't create before, create it
chunk_t *get_chunk(const ivector3_t &coord);
// If the chunk doesn't exist, return NULL
chunk_t *access_chunk(const ivector3_t &coord);
// The amount of loaded chunks
chunk_t **get_active_chunks(uint32_t *count);
// The amount of chunks that were modified in the last timestep
chunk_t **get_modified_chunks(uint32_t *count);
// Adds a sphere through modifying voxels
void generate_sphere(const vector3_t &ws_center, float ws_radius);

enum terraform_type_t { TT_DESTROY, TT_BUILD };

struct terraform_package_t {
    vector3_t ws_position;
    bool ray_hit_terrain;
};

// This will return a terraforming package to use in the terraform function
terraform_package_t cast_terrain_ray(const vector3_t &ws_ray_start, const vector3_t &ws_ray_direction, float max_reach);
// Terraforms at a position that was specified in the terraform package
bool terraform(terraform_type_t type, terraform_package_t package, float radius, float speed, float dt);

enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };

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

// This will perform a collide and slide physics thingy
vector3_t collide_and_slide(terrain_collision_t *collision);

void track_modification_history();
void stop_track_modification_history();
void reset_modification_tracker();
