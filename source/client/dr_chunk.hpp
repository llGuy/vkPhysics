#pragma once

#include <vk.hpp>
#include <vkph_chunk.hpp>
#include <vkph_state.hpp>

typedef vk::mesh_render_data_t chunk_render_data_t;

struct uncompressed_chunk_mesh_vertex_t {
    vector3_t position;
    uint32_t color;
};

struct compressed_chunk_mesh_vertex_t {
    // 12 bits for the coordinate of the nearest voxel
    // 20 bits: 8 (normalized x of the vertex), 8 (normalized y of the vertex), 4 (first 4 bits of z)
    uint32_t low;
    // 4 bits for the second half of z, and 8 bits for the color
    uint32_t high;
    // Find way to avoid wasting 20 bits !
};

struct chunk_render_t {
    vk::mesh_t mesh;

    chunk_render_data_t render_data;

    uint32_t id;
};

// Temporary, just for refactoring
chunk_render_t *dr_chunk_render_init(const vkph::chunk_t *c, const vector3_t &ws_position);

void dr_destroy_chunk_render(chunk_render_t *render);

uint32_t dr_generate_chunk_verts(
    uint8_t surface_level,
    const vkph::chunk_t *c,
    vector3_t *mesh_vertices,
    const vkph::state_t *state);

// Updates gpu buffers, etc...
void dr_update_chunk_draw_rsc(
    VkCommandBuffer command_buffer,
    uint8_t surface_level,
    vkph::chunk_t *c,
    compressed_chunk_mesh_vertex_t *verts,
    const vkph::state_t *state);
