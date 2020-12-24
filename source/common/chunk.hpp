#pragma once

#include "common/containers.hpp"
#include "tools.hpp"
#include "constant.hpp"
#include <bits/stdint-uintn.h>

ivector3_t space_world_to_voxel(const vector3_t &ws_position);
ivector3_t space_voxel_to_chunk(const ivector3_t &vs_position);
vector3_t space_chunk_to_world(const ivector3_t &chunk_coord);
ivector3_t space_voxel_to_local_chunk(const ivector3_t &vs_position);
uint32_t get_voxel_index(uint32_t x, uint32_t y, uint32_t z);

uint32_t hash_chunk_coord(const ivector3_t &coord);
// If on client side, client will have to handle destroying the rendering resources of the chunk
void destroy_chunk(chunk_t *chunk);
// Adds a sphere through modifying voxels
enum generation_type_t { GT_ADDITIVE, GT_DESTRUCTIVE, GT_INVALID } ;
void generate_sphere(const vector3_t &ws_center, float ws_radius, float max_value, generation_type_t type, voxel_color_t color);
void generate_hollow_sphere(const vector3_t &ws_center, float ws_radius, float max_value, generation_type_t type, voxel_color_t color);
void generate_platform(const vector3_t &position, float width, float depth, generation_type_t type, voxel_color_t color);
void generate_math_equation(const vector3_t &ws_center, const vector3_t &ws_extent, float(*equation)(float x, float y, float z), generation_type_t type, voxel_color_t color);

enum terraform_type_t { TT_DESTROY, TT_BUILD };

struct terraform_package_t {
    // Voxel position
    vector3_t ws_position;
    // Triangle contact point in world space
    vector3_t ws_contact_point;
    bool ray_hit_terrain;
    voxel_color_t color;
};

// This will return a terraforming package to use in the terraform function
terraform_package_t cast_terrain_ray(const vector3_t &ws_ray_start, const vector3_t &ws_ray_direction, float max_reach, voxel_color_t color);
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
void check_ray_terrain_collision(terrain_collision_t *collision);
