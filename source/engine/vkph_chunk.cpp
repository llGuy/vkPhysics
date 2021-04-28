#include "vkph_chunk.hpp"
#include "vkph_constant.hpp"

namespace vkph {

void chunk_t::init(uint32_t chunk_stack_index, const ivector3_t &cchunk_coord) {
    xs_bottom_corner = cchunk_coord * CHUNK_EDGE_LENGTH;
    chunk_coord = cchunk_coord;
    chunk_stack_index = chunk_stack_index;

    flags.made_modification = 0;
    flags.has_to_update_vertices = 0;
    flags.active_vertices = 0;
    flags.modified_marker = 0;
    flags.index_of_modification_struct = 0;

    memset(voxels, 0, sizeof(voxel_t) * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);

    history.modification_count = 0;
    memset(history.modification_pool, CHUNK_SPECIAL_VALUE, CHUNK_VOXEL_COUNT);

    render = NULL;

    players_in_chunk.init();
}

void chunk_t::destroy() {
    players_in_chunk.destroy();
}

ivector3_t space_world_to_voxel(const vector3_t &ws_position) {
    return (ivector3_t)(glm::floor(ws_position));
}

ivector3_t space_voxel_to_chunk(const ivector3_t &vs_position) {
    vector3_t from_origin = (vector3_t)vs_position;
    vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
    return (ivector3_t)xs_sized;
}

vector3_t space_chunk_to_world(const ivector3_t &chunk_coord) {
    vector3_t ws_coord = (vector3_t)(chunk_coord);
    return ws_coord * (float)CHUNK_EDGE_LENGTH;
}

ivector3_t space_voxel_to_local_chunk(const ivector3_t &vs_position) {
    vector3_t from_origin = (vector3_t)vs_position;
    vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
    return (ivector3_t)(from_origin - xs_sized * (float)CHUNK_EDGE_LENGTH);
}

uint32_t get_voxel_index(uint32_t x, uint32_t y, uint32_t z) {
    return z * (CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH) + y * CHUNK_EDGE_LENGTH + x;
}

uint32_t hash_chunk_coord(
    const ivector3_t &coord) {
    struct {
        union {
            struct {
                uint32_t padding: 2;
                uint32_t x: 10;
                uint32_t y: 10;
                uint32_t z: 10;
            };
            uint32_t value;
        };
    } hasher;

    hasher.value = 0;

    hasher.x = *(uint32_t *)(&coord.x);
    hasher.y = *(uint32_t *)(&coord.y);
    hasher.z = *(uint32_t *)(&coord.z);

    return (uint32_t)hasher.value;
}

}
