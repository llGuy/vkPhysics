#include "chunk.hpp"
#include "constant.hpp"
#include "containers.hpp"

static stack_container_t<chunk_t *> chunks;

static hash_table_t<uint32_t, 300, 30, 10> chunk_indices;

static uint32_t max_modified_chunks;

static uint32_t modified_chunk_count;

static chunk_t **modified_chunks;

void chunk_memory_init() {
    chunk_indices.init();
    chunks.init(CHUNK_MAX_LOADED_COUNT);

    max_modified_chunks = CHUNK_MAX_LOADED_COUNT / 2;
    modified_chunk_count = 0;
    modified_chunks = FL_MALLOC(chunk_t *, max_modified_chunks);
}

void clear_chunks() {
    if (chunks.data_count) {
        for (uint32_t i = 0; i < chunks.data_count; ++i) {
            chunks[i] = NULL;
        }

        chunks.clear();
        chunk_indices.clear();
    }
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

void chunk_init(chunk_t *chunk, uint32_t chunk_stack_index, const ivector3_t &chunk_coord) {
    chunk->xs_bottom_corner = chunk_coord * CHUNK_EDGE_LENGTH;
    chunk->chunk_coord = chunk_coord;
    chunk->chunk_stack_index = chunk_stack_index;

    chunk->flags.made_modification = 0;
    chunk->flags.has_to_update_vertices = 0;
    chunk->flags.active_vertices = 0;
    chunk->flags.modified_marker = 0;
    chunk->flags.index_of_modification_struct = 0;

    memset(chunk->voxels, 0, sizeof(uint8_t) * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);

    chunk->history.modification_count = 0;
    memset(chunk->history.modification_pool, CHUNK_SPECIAL_VALUE, CHUNK_VOXEL_COUNT);

    chunk->render = NULL;
}

void destroy_chunk(chunk_t *chunk) {
    FL_FREE(chunk);
}

static uint32_t s_hash_chunk_coord(
    const ivector3_t &coord) {
    // static std::hash<glm::ivec3> hasher;

    // TODO: Investigate if this is portable to different systems (endianness??)
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

chunk_t *get_chunk(
    const ivector3_t &coord) {
    uint32_t hash = s_hash_chunk_coord(coord);
    uint32_t *index = chunk_indices.get(hash);
    
    if (index) {
        // Chunk was already added
        return chunks[*index];
    }
    else {
        uint32_t i = chunks.add();
        chunk_t *&chunk = chunks[i];
        chunk = FL_MALLOC(chunk_t, 1);
        chunk_init(chunk, i, coord);

        chunk_indices.insert(hash, i);

        return chunk;
    }
}

chunk_t *access_chunk(
    const ivector3_t &coord) {
    uint32_t hash = s_hash_chunk_coord(coord);
    uint32_t *index = chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return chunks[*index];
    }
    else {
        return NULL;
    }
}

chunk_t **get_active_chunks(
    uint32_t *count) {
    *count = chunks.data_count;
    return chunks.data;
}

chunk_t **get_modified_chunks(
    uint32_t *count) {
    *count = modified_chunk_count;
    return modified_chunks;
}

void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius) {
    ivector3_t vs_center = space_world_to_voxel(ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = space_voxel_to_chunk(vs_center);

    chunk_t *current_chunk = get_chunk(
        current_chunk_coord);

    current_chunk->flags.has_to_update_vertices = 1;

    int32_t diameter = (int32_t)ws_radius * 2 + 1;

    int32_t start_z = vs_center.z - (int32_t)ws_radius;
    int32_t start_y = vs_center.y - (int32_t)ws_radius;
    int32_t start_x = vs_center.x - (int32_t)ws_radius;

    float radius_squared = ws_radius * ws_radius;

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                ivector3_t vs_position = ivector3_t(x, y, z);
                vector3_t vs_float = vector3_t((float)x, (float)y, (float)z);
                vector3_t vs_diff_float = vs_float - vs_float_center;

                float distance_squared = glm::dot(vs_diff_float, vs_diff_float);

                if (distance_squared <= radius_squared) {
                    ivector3_t c = space_voxel_to_chunk(vs_position);

                    ivector3_t chunk_origin_diff = vs_position - current_chunk_coord * (int32_t)CHUNK_EDGE_LENGTH;
                    //if (c.x == current_chunk_coord.x && c.y == current_chunk_coord.y && c.z == current_chunk_coord.z) {
                    if (chunk_origin_diff.x >= 0 && chunk_origin_diff.x < 16 &&
                        chunk_origin_diff.y >= 0 && chunk_origin_diff.y < 16 &&
                        chunk_origin_diff.z >= 0 && chunk_origin_diff.z < 16) {
                        // Is within current chunk boundaries
                        float proportion = 1.0f - (distance_squared / radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

                        //current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);

                        uint8_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * (float)CHUNK_MAX_VOXEL_VALUE_I);
                        if (*v < new_value) {
                            *v = new_value;
                        }
                    }
                    else {
                        ivector3_t c = space_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = get_chunk(c);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = 1.0f - (distance_squared / radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        uint8_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * (float)CHUNK_MAX_VOXEL_VALUE_I);
                        if (*v < new_value) {
                            *v = new_value;
                        }
                    }
                }
            }
        }
    }
}
