#include "vk_load_mesh.hpp"

#include <math.hpp>
#include <tools.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

namespace vk {

vector3_t *get_vertices(
    uint32_t vertices_count,
    serialiser_t *serialiser,
    vector3_t *p) {
    vector3_t *vertices;

    if (p) {
        vertices = p;
    }
    else {
        vertices = LN_MALLOC(vector3_t, vertices_count);
    }

    for (uint32_t i = 0; i < vertices_count; ++i) {
        vertices[i] = serialiser->deserialise_vector3();
    }

    return vertices;
}

vector3_t *get_normals(
    uint32_t normals_count,
    serialiser_t *serialiser,
    vector3_t *p) {
    vector3_t *normals;

    if (p) {
        normals = p;
    }
    else {
        normals = LN_MALLOC(vector3_t, normals_count);
    }

    for (uint32_t i = 0; i < normals_count; ++i) {
        normals[i] = serialiser->deserialise_vector3();
    }

    return normals;
}

vector2_t *get_tcoords(
    uint32_t tcoords_count,
    serialiser_t *serialiser,
    vector2_t *p) {
    vector2_t *tcoords;

    if (p) {
        tcoords = p;
    }
    else {
        tcoords = LN_MALLOC(vector2_t, tcoords_count);
    }

    for (uint32_t i = 0; i < tcoords_count; ++i) {
        tcoords[i].x = serialiser->deserialise_float32();
        tcoords[i].y = serialiser->deserialise_float32();
    }

    return tcoords;
}

vector4_t *get_joint_weights(
    uint32_t joint_weights_count,
    serialiser_t *serialiser,
    vector4_t *p) {
    vector4_t *joint_weights;

    if (p) {
        joint_weights = p;
    }
    else {
        joint_weights = LN_MALLOC(vector4_t, joint_weights_count);
    }

    for (uint32_t i = 0; i < joint_weights_count; ++i) {
        joint_weights[i].x = serialiser->deserialise_float32();
        joint_weights[i].y = serialiser->deserialise_float32();
        joint_weights[i].z = serialiser->deserialise_float32();
        joint_weights[i].w = serialiser->deserialise_float32();
        float sum = joint_weights[i].x + joint_weights[i].y + joint_weights[i].z + joint_weights[i].w;
        float q = 1.0f / sum;
        joint_weights[i] *= q;
    }

    return joint_weights;
}

ivector4_t *get_joint_ids(
    uint32_t joint_ids_count,
    serialiser_t *serialiser,
    ivector4_t *p) {
    ivector4_t *joint_ids;

    if (p) {
        joint_ids = p;
    }
    else {
        joint_ids = LN_MALLOC(ivector4_t, joint_ids_count);
    }

    for (uint32_t i = 0; i < joint_ids_count; ++i) {
        joint_ids[i].x = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].y = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].z = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].w = (int32_t)serialiser->deserialise_uint32();
    }

    return joint_ids;
}

uint32_t *get_indices(
    uint32_t indices_count,
    serialiser_t *serialiser) {
    uint32_t *indices = LN_MALLOC(uint32_t, indices_count);

    for (uint32_t i = 0; i < indices_count; ++i) {
        indices[i] = serialiser->deserialise_uint32();
    }

    return indices;
}

}
