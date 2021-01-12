#include "net_chunk_tracker.hpp"

#include <vkph_chunk.hpp>
#include <vkph_state.hpp>
#include <allocators.hpp>

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

void deserialise_chunk_modification_colors_from_array(
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
    chunk_modifications_t *chunk_modifications = lnmalloc<chunk_modifications_t>(*modification_count);

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

uint32_t fill_chunk_modification_array_with_initial_values(chunk_modifications_t *modifications, const vkph::state_t *state) {
    uint32_t modified_chunk_count = 0;
    const vkph::chunk_t **chunks = state->get_modified_chunks(&modified_chunk_count);

    uint32_t current = 0;
            
    for (uint32_t c_index = 0; c_index < modified_chunk_count; ++c_index) {
        chunk_modifications_t *cm_ptr = &modifications[current];
        const vkph::chunk_t *c_ptr = chunks[c_index];
        const vkph::chunk_history_t *h_ptr = &chunks[c_index]->history;

        if (h_ptr->modification_count == 0) {
            // Chunk doesn't actually have modifications, it was just flagged
        }
        else {
            cm_ptr->x = c_ptr->chunk_coord.x;
            cm_ptr->y = c_ptr->chunk_coord.y;
            cm_ptr->z = c_ptr->chunk_coord.z;
            cm_ptr->modified_voxels_count = h_ptr->modification_count;
            for (uint32_t v_index = 0; v_index < cm_ptr->modified_voxels_count; ++v_index) {
                cm_ptr->modifications[v_index].index = (uint16_t)h_ptr->modification_stack[v_index];
                cm_ptr->modifications[v_index].initial_value = h_ptr->modification_pool[cm_ptr->modifications[v_index].index];
                cm_ptr->modifications[v_index].final_value = c_ptr->voxels[cm_ptr->modifications[v_index].index].value;
                cm_ptr->colors[v_index] = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
            }

            ++current;
        }
    }

    return current;
}

uint32_t fill_chunk_modification_array_with_colors(chunk_modifications_t *modifications, const vkph::state_t *state) {
    uint32_t modified_chunk_count = 0;
    const vkph::chunk_t **chunks = state->get_modified_chunks(&modified_chunk_count);

    uint32_t current = 0;
            
    for (uint32_t c_index = 0; c_index < modified_chunk_count; ++c_index) {
        chunk_modifications_t *cm_ptr = &modifications[current];
        const vkph::chunk_t *c_ptr = chunks[c_index];
        const vkph::chunk_history_t *h_ptr = &chunks[c_index]->history;

        if (h_ptr->modification_count == 0) {
            // Chunk doesn't actually have modifications, it was just flagged
        }
        else {
            cm_ptr->x = c_ptr->chunk_coord.x;
            cm_ptr->y = c_ptr->chunk_coord.y;
            cm_ptr->z = c_ptr->chunk_coord.z;
            cm_ptr->modified_voxels_count = h_ptr->modification_count;
            for (uint32_t v_index = 0; v_index < cm_ptr->modified_voxels_count; ++v_index) {
                cm_ptr->modifications[v_index].index = (uint16_t)h_ptr->modification_stack[v_index];
                // Difference is here because the server will just send the voxel values array, not the colors array
                cm_ptr->modifications[v_index].color = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
                cm_ptr->modifications[v_index].final_value = c_ptr->voxels[cm_ptr->modifications[v_index].index].value;
                cm_ptr->colors[v_index] = c_ptr->voxels[cm_ptr->modifications[v_index].index].color;
            }

            ++current;
        }
    }

    return current;
}

}
