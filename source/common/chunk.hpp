#pragma once

#include "common/containers.hpp"
#include "tools.hpp"
#include "constant.hpp"
#include <bits/stdint-uintn.h>


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
