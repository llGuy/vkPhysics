#pragma once

#include <renderer/renderer.hpp>

typedef mesh_render_data_t chunk_render_data_t;

struct chunk_render_t {
    mesh_t mesh;
    chunk_render_data_t render_data;
};

// Temporary, just for refactoring
[[nodiscard]] chunk_render_t *dr_chunk_render_init(const struct chunk_t *c, const vector3_t &ws_position);
void dr_destroy_chunk_render(chunk_render_t *render);
uint32_t dr_generate_chunk_verts(uint8_t surface_level, const chunk_t *c, vector3_t *mesh_vertices);
// Updates gpu buffers, etc...
void dr_update_chunk_draw_rsc(VkCommandBuffer command_buffer, uint8_t surface_level, chunk_t *c, vector3_t *verts = NULL);
