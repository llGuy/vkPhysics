#include "net_chunk_tracker.hpp"

#include <common/allocators.hpp>

namespace net {

void serialise_chunk_modification_meta_info(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    serialiser->serialise_int16(c->x);
    serialiser->serialise_int16(c->y);
    serialiser->serialise_int16(c->z);
    serialiser->serialise_uint32(c->modified_voxels_count);
}

static void s_serialise_chunk_modification_values_without_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

static void s_serialise_chunk_modification_values_with_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->color);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

void serialise_chunk_modification_values_with_initial_values(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        serialiser->serialise_uint16(v_ptr->index);
        serialiser->serialise_uint8(v_ptr->initial_value);
        serialiser->serialise_uint8(v_ptr->final_value);
    }
}

void serialise_chunk_modification_colors_from_array(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        serialiser->serialise_uint8(c->colors[v]);
    }
}

void serialise_chunk_modifications(
    chunk_modifications_t *modifications,
    uint32_t modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t cst) {
    serialiser->serialise_uint32(modification_count);
    
    // Yes I know this is stupid because color is a bool
    if (cst == CST_SERIALISE_SEPARATE_COLOR) {
        for (uint32_t i = 0; i < modification_count; ++i) {
            chunk_modifications_t *c = &modifications[i];
            serialise_chunk_modification_meta_info(serialiser, c);
            s_serialise_chunk_modification_values_without_colors(serialiser, c);
            serialise_chunk_modification_colors_from_array(serialiser, c);
        }
    }
    else {
        for (uint32_t i = 0; i < modification_count; ++i) {
            chunk_modifications_t *c = &modifications[i];
            serialise_chunk_modification_meta_info(serialiser, c);
            s_serialise_chunk_modification_values_with_colors(serialiser, c);
        }
    }
}

void deserialise_chunk_modification_meta_info(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    c->x = serialiser->deserialise_int16();
    c->y = serialiser->deserialise_int16();
    c->z = serialiser->deserialise_int16();
    c->modified_voxels_count = serialiser->deserialise_uint32();
}

static void s_deserialise_chunk_modification_values_without_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

static void s_deserialise_chunk_modification_values_with_colors(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->color = serialiser->deserialise_uint8();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

void deserialise_chunk_modification_values_with_initial_values(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        voxel_modification_t *v_ptr =  &c->modifications[v];
        v_ptr->index = serialiser->deserialise_uint16();
        v_ptr->initial_value = serialiser->deserialise_uint8();
        v_ptr->final_value = serialiser->deserialise_uint8();
    }
}

static void s_deserialise_chunk_modification_colors_from_array(
    serialiser_t *serialiser,
    chunk_modifications_t *c) {
    for (uint32_t v = 0; v < c->modified_voxels_count; ++v) {
        c->colors[v] = serialiser->deserialise_uint8();
    }
}

chunk_modifications_t *deserialise_chunk_modifications(
    uint32_t *modification_count,
    serialiser_t *serialiser,
    color_serialisation_type_t color) {
    *modification_count = serialiser->deserialise_uint32();
    chunk_modifications_t *chunk_modifications = LN_MALLOC(chunk_modifications_t, *modification_count);

    if (color == CST_SERIALISE_SEPARATE_COLOR) {
        for (uint32_t i = 0; i < *modification_count; ++i) {
            chunk_modifications_t *c = &chunk_modifications[i];
            deserialise_chunk_modification_meta_info(serialiser, c);
            s_deserialise_chunk_modification_values_without_colors(serialiser, c);
            deserialise_chunk_modification_colors_from_array(serialiser, c);
        }
    }
    else {
        for (uint32_t i = 0; i < *modification_count; ++i) {
            chunk_modifications_t *c = &chunk_modifications[i];
            deserialise_chunk_modification_meta_info(serialiser, c);
            s_deserialise_chunk_modification_values_with_colors(serialiser, c);
        }
    }

    return chunk_modifications;
}

}
