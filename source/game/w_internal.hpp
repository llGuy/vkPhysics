#pragma once

#include <common/tools.hpp>
#include <common/containers.hpp>
#include <renderer/renderer.hpp>

#define CHUNK_EDGE_LENGTH 16
#define MAX_VERTICES_PER_CHUNK 5 * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1)

// Push constant
struct chunk_render_data_t {
    matrix4_t model_matrix;
};

struct chunk_render_t {
    mesh_t mesh;
    chunk_render_data_t render_data;
    gpu_buffer_t chunk_vertices_gpu_buffer;
    bool should_do_gpu_sync = 0;
};

struct chunk_t {
    uint32_t chunk_stack_index;
    ivector3_t xs_bottom_corner;
    ivector3_t chunk_coord;

    uint8_t voxels[CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH];

    chunk_render_t *render;
};

void w_chunk_init(
    chunk_t *chunk,
    uint32_t chunk_stack_index,
    const ivector3_t &chunk_coord);

void w_chunk_render_init(
    chunk_t *chunk,
    const vector3_t &ws_position,
    const vector3_t &ws_size);

// Returns NULL
// Usage: chunk = w_destroy_chunk(chun);
chunk_t *w_destroy_chunk(
    chunk_t *chunk);

struct chunk_pointer_t {
    // Index into the loaded_chunk_list
    uint32_t chunk_index;
};

// Max loaded chunks for now (loaded chunk = chunk with active voxels)
#define MAX_LOADED_CHUNKS 1000

struct chunk_world_t {
    // Number of chunks to be loaded in x, y and z directions from the player's current chunk position
    uint32_t loaded_radius;
    // Array of size loaded_radius * loaded_radius * loaded_radius
    uint32_t *chunk_indices;

    // List of chunks
    // Works like a stack
    uint32_t stack_pointer;
    chunk_t **loaded_chunk_stack;
};

void w_chunk_world_init(
    chunk_world_t *world,
    uint32_t loaded_radius);


