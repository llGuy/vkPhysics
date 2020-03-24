#include "w_internal.hpp"
#include <common/tools.hpp>
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

uint32_t w_hash_chunk_coord(
    const ivector3_t &coord) {
    uint32_t result_hash = 2166136261u ^ coord[0] * 16777619u;
    result_hash ^= coord[1] * 16777619u;
    result_hash ^= coord[2] * 16777619u;
    
    return result_hash;
}

void w_chunk_world_init(
    chunk_world_t *world,
    uint32_t loaded_radius) {
    world->loaded_radius = loaded_radius;

    world->stack_pointer = 0;
    world->loaded_chunk_stack = FL_MALLOC(chunk_t *, MAX_LOADED_CHUNKS);
}

void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius,
    chunk_world_t *world) {
    ivector3_t vs_center = w_convert_world_to_voxel(ws_center);

    ivector3_t current_chunk = w_convert_voxel_to_chunk(vs_center);

    
}

ivector3_t w_convert_world_to_voxel(
    const vector3_t &ws_position) {
    return (ivector3_t)(glm::floor(ws_position));
}

ivector3_t w_convert_voxel_to_chunk(
    const ivector3_t &vs_position) {
    vector3_t from_origin = (vector3_t)vs_position;
    vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
    return (ivector3_t)xs_sized;
}

ivector3_t w_convert_voxel_to_local_chunk(
    const ivector3_t &vs_position) {
    vector3_t from_origin = (vector3_t)vs_position;
    vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
    return (ivector3_t)(from_origin - xs_sized * (float)CHUNK_EDGE_LENGTH);
}

chunk_t *w_get_chunk(
    const ivector3_t &coord,
    chunk_world_t *world) {
    
}
