#pragma once

#include <stdint.h>
#include <math.hpp>

struct serialiser_t;

namespace vk {

struct header_t {
    uint32_t vertices_count;
    uint32_t normals_count;
    uint32_t tcoords_count;
    uint32_t joint_weights_count;
    uint32_t joint_ids_count;
    // UINT32_T
    uint32_t indices_count;
};

vector3_t *get_vertices(uint32_t vertices_count, serialiser_t *serialiser, vector3_t *p = NULL);
vector3_t *get_normals(uint32_t normals_count, serialiser_t *serialiser, vector3_t *p = NULL);
vector2_t *get_tcoords(uint32_t tcoords_count, serialiser_t *serialiser, vector2_t *p = NULL);
vector4_t *get_joint_weights(uint32_t joint_weights_count, serialiser_t *serialiser, vector4_t *p = NULL);
ivector4_t *get_joint_ids(uint32_t joint_ids_count, serialiser_t *serialiser, ivector4_t *p = NULL);
uint32_t *get_indices(uint32_t indices_count, serialiser_t *serialiser);

}
