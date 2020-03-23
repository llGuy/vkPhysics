#include "w_internal.hpp"
#include <renderer/renderer.hpp>

void w_chunk_init(
    chunk_t *chunk,
    uint32_t chunk_stack_index,
    const ivector3_t &chunk_coord) {
    chunk->xs_bottom_corner = chunk_coord * CHUNK_EDGE_LENGTH;
    chunk->chunk_coord = chunk_coord;
    chunk->chunk_stack_index = chunk_stack_index;

    memset(chunk->voxels, 0, sizeof(uint8_t) * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);

    chunk->render = NULL;
}

void w_chunk_render_init(
    chunk_t *chunk,
    const vector3_t &ws_position,
    const vector3_t &ws_size) {
    chunk->render = FL_MALLOC(chunk_render_t, 1);

    uint32_t buffer_size = sizeof(vector3_t) * MAX_VERTICES_PER_CHUNK;

    chunk->render->chunk_vertices_gpu_buffer = create_gpu_buffer(
        buffer_size,
        NULL,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
    chunk->render->should_do_gpu_sync = 1;

    chunk->render->render_data.model_matrix = glm::scale(ws_size) * glm::translate(ws_position);
}

void w_destroy_chunk_render(
    chunk_t *chunk) {
    if (chunk->render) {
        destroy_gpu_buffer(chunk->render->chunk_vertices_gpu_buffer);
        FL_FREE(chunk->render);
        chunk->render = NULL;
    }
}

chunk_t *w_destroy_chunk(
    chunk_t *chunk) {
    w_destroy_chunk_render(
        chunk);

    FL_FREE(chunk);

    return NULL;
}

void w_chunk_world_init(
    chunk_world_t *world,
    uint32_t loaded_radius) {
    world->loaded_radius = loaded_radius;
    world->chunk_indices = FL_MALLOC(
        uint32_t,
        (world->loaded_radius * 2 - 1) * (world->loaded_radius * 2 - 1) * (world->loaded_radius * 2 - 1));

    world->stack_pointer = 0;
    world->loaded_chunk_stack = FL_MALLOC(chunk_t *, MAX_LOADED_CHUNKS);
}

void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius,
    chunk_world_t *world) {
    
}
