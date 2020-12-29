#pragma once

#include "vkph_voxel.hpp"

#include <math.hpp>

namespace vkph {

struct state_t;

/*
  Some functions to help in terraform (especially in the terrain editor).
  Creates structures through setting voxel values.
 */
enum generation_type_t { GT_ADDITIVE, GT_DESTRUCTIVE, GT_INVALID } ;

/*
  This applies for both hollow and non-hollow spheres.
 */
struct sphere_create_info_t {
    vector3_t ws_center;
    float ws_radius;
    float max_value;
    generation_type_t type;
    voxel_color_t color;
};

struct platform_create_info_t {
    const vector3_t &position;
    float width;
    float depth;
    generation_type_t type;
    voxel_color_t color;
};

struct math_equation_create_info_t {
    const vector3_t &ws_center;
    const vector3_t &ws_extent;
    float(*equation)(float x, float y, float z);
    generation_type_t type;
    voxel_color_t color;
};

enum terraform_type_t { TT_DESTROY, TT_BUILD };

struct terraform_package_t {
    // Voxel position
    vector3_t ws_position;
    // Triangle contact point in world space
    vector3_t ws_contact_point;
    bool ray_hit_terrain;
    voxel_color_t color;
};

struct terraform_info_t {
    terraform_type_t type;
    terraform_package_t *package;
    float radius;
    float speed;
    float dt;
};

}
