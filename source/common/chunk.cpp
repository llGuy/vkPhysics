#include "math.hpp"
#include "chunk.hpp"
#include "constant.hpp"
#include "containers.hpp"

static stack_container_t<chunk_t *> chunks;

static hash_table_t<uint32_t, 300, 30, 10> chunk_indices;

static uint32_t max_modified_chunks;

static uint32_t modified_chunk_count;

static chunk_t **modified_chunks;

static struct {
    uint8_t track_history: 1;
} flags;

void chunk_memory_init() {
    chunk_indices.init();
    chunks.init(CHUNK_MAX_LOADED_COUNT);

    max_modified_chunks = CHUNK_MAX_LOADED_COUNT / 2;
    modified_chunk_count = 0;
    modified_chunks = FL_MALLOC(chunk_t *, max_modified_chunks);

    flags.track_history = 1;
}

void clear_chunks() {
    if (chunks.data_count) {
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

enum { B8_R_MAX = 0b111, B8_G_MAX = 0b111, B8_B_MAX = 0b11 };

vector3_t b8_color_to_v3(voxel_color_t color) {
    uint8_t r_b8 = color >> 5;
    uint8_t g_b8 = (color >> 2) & 0b111;
    uint8_t b_b8 = (color) & 0b11;

    float r_f32 = (float)(r_b8) / (float)(B8_R_MAX);
    float g_f32 = (float)(g_b8) / (float)(B8_G_MAX);
    float b_f32 = (float)(b_b8) / (float)(B8_B_MAX);

    return vector3_t(r_f32, g_f32, b_f32);
}

voxel_color_t v3_color_to_b8(const vector3_t &color) {
    float r = color.r * (float)(B8_R_MAX);
    float g = color.g * (float)(B8_G_MAX);
    float b = color.b * (float)(B8_B_MAX);

    return ((uint8_t)r << 5) + ((uint8_t)g << 2) + ((uint8_t)b);
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

    memset(chunk->voxels, 0, sizeof(voxel_t) * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH);

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

void generate_hollow_sphere(
    const vector3_t &ws_center,
    float ws_radius,
    float max_value,
    generation_type_t type,
    voxel_color_t color) {
    float (* generation_proc)(float distance_squared, float radius_squared);
    switch(type) {
    case GT_ADDITIVE: {
        generation_proc =
            [] (float distance_squared, float radius_squared) {
                float diff = abs(distance_squared - radius_squared);
                if (diff < 120.0f) {
                    return 1.0f - (diff / 120.0f);
                }
                else {
                    return 0.0f;
                }
            };
    } break;

    case GT_DESTRUCTIVE: {
        generation_proc =
            [] (float distance_squared, float radius_squared) {
                return 0.0f;
            };

    } break;
    }

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
    float smaller_radius_squared = (ws_radius - 10) * (ws_radius - 10);

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
                        float proportion = generation_proc(distance_squared, smaller_radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

                        //current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * max_value);
                        if (v->value < new_value) {
                            v->value = new_value;
                            v->color = color;
                        }
                    }
                    else {
                        ivector3_t c = space_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = get_chunk(c);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = generation_proc(distance_squared, smaller_radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * max_value);
                        if (v->value < new_value) {
                            v->value = new_value;
                            v->color = color;
                        }
                    }
                }
            }
        }
    }
}

void generate_sphere(
    const vector3_t &ws_center,
    float ws_radius,
    float max_value,
    generation_type_t type,
    voxel_color_t color) {
    float (* generation_proc)(float distance_squared, float radius_squared);
    switch(type) {
    case GT_ADDITIVE: {
        generation_proc =
            [] (float distance_squared, float radius_squared) {
                return 1.0f - (distance_squared / radius_squared);
            };
    } break;

    case GT_DESTRUCTIVE: {
        generation_proc =
            [] (float distance_squared, float radius_squared) {
                return 0.0f;
            };

    } break;
    }

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
                        float proportion = generation_proc(distance_squared, radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

                        //current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * max_value);
                        // if (*v < new_value) {
                            v->value = new_value;
                            v->color = color;
                            // }
                    }
                    else {
                        ivector3_t c = space_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = get_chunk(c);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = generation_proc(distance_squared, radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * max_value);
                        // if (*v < new_value) {
                            v->value = new_value;
                            v->color = color;

                            // }
                    }
                }
            }
        }
    }
}

void generate_platform(const vector3_t &position, float width, float depth, generation_type_t type, voxel_color_t color) {
    uint8_t (* generation_proc)();
    switch (type) {
    case GT_ADDITIVE: {
        generation_proc =
            [] () -> uint8_t {
                return 80;
            };
    } break;

    case GT_DESTRUCTIVE: {
        generation_proc =
            [] () -> uint8_t {
                return 0;
            };

    } break;
    }

    for (int32_t z = position.z - depth / 2; z < position.z + depth / 2; ++z) {
        for (int32_t x = position.x - width / 2; x < position.x + width / 2; ++x) {
            ivector3_t voxel_coord = ivector3_t((float)x, -2.0f, (float)z);
            ivector3_t chunk_coord = space_voxel_to_chunk(voxel_coord);
            chunk_t *chunk = get_chunk(chunk_coord);
            chunk->flags.has_to_update_vertices = 1;
            ivector3_t local_coord = space_voxel_to_local_chunk(voxel_coord);
            uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
            chunk->voxels[index].value = generation_proc();
            chunk->voxels[index].color = color;
        }
    }
}

void generate_math_equation(
    const vector3_t &ws_center,
    const vector3_t &extent,
    float (* equation)(float x, float y, float z),
    generation_type_t type,
    voxel_color_t color) {
    uint8_t (* generation_proc)(float equation_result);
    switch (type) {
    case GT_ADDITIVE: {
        generation_proc =
            [] (float equation_result) -> uint8_t {
                return (uint8_t)(150.0f * equation_result);
            };
    } break;

    case GT_DESTRUCTIVE: {
        generation_proc =
            [] (float equation_result) -> uint8_t {
                return 0;
            };
    } break;
    }

    for (int32_t z = ws_center.z - extent.z / 2; z < ws_center.z + extent.z / 2; ++z) {
        for (int32_t y = ws_center.y - extent.y / 2; y < ws_center.y + extent.y / 2; ++y) {
            for (int32_t x = ws_center.x - extent.x / 2; x < ws_center.x + extent.x / 2; ++x) {
                float c = equation(x - ws_center.x, y - ws_center.y, z - ws_center.z);

                if (c > 0.0f) {
                    ivector3_t voxel_coord = ivector3_t((float)x, (float)y, (float)z);
                    ivector3_t chunk_coord = space_voxel_to_chunk(voxel_coord);
                    chunk_t *chunk = get_chunk(chunk_coord);
                    chunk->flags.has_to_update_vertices = 1;
                    ivector3_t local_coord = space_voxel_to_local_chunk(voxel_coord);
                    uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
                    chunk->voxels[index].value = generation_proc(c);
                    chunk->voxels[index].color = color;
                }
            }
        }
    }
}

terraform_package_t cast_terrain_ray(
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach) {
    vector3_t vs_position = ws_ray_start;
    vector3_t vs_dir = ws_ray_direction;

    static const float PRECISION = 1.0f / 10.0f;
    
    vector3_t vs_step = vs_dir * max_reach * PRECISION;

    float max_reach_squared = max_reach * max_reach;

    terraform_package_t package = {};
    package.ray_hit_terrain = 0;

    for (; glm::dot(vs_position - ws_ray_start, vs_position - ws_ray_start) < max_reach_squared; vs_position += vs_step) {
        ivector3_t voxel = space_world_to_voxel(vs_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];
            if (voxel_ptr->value > CHUNK_SURFACE_LEVEL) {
                package.ray_hit_terrain = 1;
                package.ws_position = vector3_t(voxel);
                package.color = v3_color_to_b8(vector3_t(0.0f, 0.5f, 1.0f));
                break;
            }
        }
    }

    return package;
}

void track_modification_history() {
    flags.track_history = 1;
}

void stop_track_modification_history() {
    flags.track_history = 0;
}

static bool s_terraform_with_history(
    terraform_type_t type,
    terraform_package_t package,
    float radius,
    float speed,
    float dt) {
    if (package.ray_hit_terrain) {
        ivector3_t voxel = space_world_to_voxel(package.ws_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (!chunk->flags.made_modification) {
            // Push this chunk onto list of modified chunks
            modified_chunks[modified_chunk_count++] = chunk;
        }
                    
        chunk->flags.made_modification = 1;
        chunk->flags.has_to_update_vertices = 1;

        float coeff = 0.0f;
        switch(type) {
                        
        case TT_DESTROY: {
            coeff = -1.0f;
        } break;
                        
        case TT_BUILD: {
            coeff = +1.0f;
        } break;
                        
        }

        float radius_squared = radius * radius;
        ivector3_t bottom_corner = voxel - ivector3_t((int32_t)radius);
        int32_t diameter = (int32_t)radius * 2 + 1;

#if 0
        // Make sure to activate chunk history
        if (chunk->history == NULL) {
            activate_chunk_history(chunk);
        }
#endif

        for (int32_t z = bottom_corner.z; z < bottom_corner.z + diameter; ++z) {
            for (int32_t y = bottom_corner.y; y < bottom_corner.y + diameter; ++y) {
                for (int32_t x = bottom_corner.x; x < bottom_corner.x + diameter; ++x) {
                    vector3_t current_voxel = vector3_t((float)x, (float)y, (float)z);
                    vector3_t diff = current_voxel - (vector3_t)voxel;
                    float distance_squared = glm::dot(diff, diff);

                    if (distance_squared <= radius_squared) {
                        ivector3_t current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                
                        if (current_local_coord.x < 0 || current_local_coord.x >= 16 ||
                            current_local_coord.y < 0 || current_local_coord.y >= 16 ||
                            current_local_coord.z < 0 || current_local_coord.z >= 16) {
                            // If the current voxel coord is out of bounds, switch chunks
                            ivector3_t chunk_coord = space_voxel_to_chunk(current_voxel);
                            chunk_t *new_chunk = get_chunk(chunk_coord);

                            chunk = new_chunk;

                            if (!chunk->flags.made_modification) {
                                // Push this chunk onto list of modified chunks
                                modified_chunks[modified_chunk_count++] = chunk;
                            }
                                        
                            chunk->flags.made_modification = 1;
                            chunk->flags.has_to_update_vertices = 1;

#if 0
                            if (chunk->history == NULL) {
                                activate_chunk_history(chunk);
                            }
#endif

                            current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                        }

                        uint32_t voxel_index = get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z);
                        voxel_t *voxel = &chunk->voxels[voxel_index];
                        uint8_t voxel_value = voxel->value;
                        float proportion = 1.0f - (distance_squared / radius_squared);

                        int32_t current_voxel_value = (int32_t)voxel->value;

                        int32_t new_value = (int32_t)(proportion * coeff * dt * speed) + current_voxel_value;

                        uint8_t *vh = &chunk->history.modification_pool[voxel_index];
                                    
                        if (new_value > (int32_t)CHUNK_MAX_VOXEL_VALUE_I) {
                            voxel_value = (int32_t)CHUNK_MAX_VOXEL_VALUE_I;
                        }
                        else if (new_value < 0) {
                            voxel_value = 0;
                        }
                        else {
                            voxel_value = (uint8_t)new_value;
                        }

                        // Didn't add to the history yet
                        if (*vh == CHUNK_SPECIAL_VALUE && voxel_value != voxel->value) {
                            *vh = voxel->value;
                            chunk->history.modification_stack[chunk->history.modification_count++] = voxel_index;
                        }
                                    
                        voxel->value = voxel_value;
                        voxel->color = package.color;
                    }
                }
            }
        }

        return 1;
    }

    return 0;
}

static bool s_terraform_without_history(
    terraform_type_t type,
    terraform_package_t package,
    float radius,
    float speed,
    float dt) {
    if (package.ray_hit_terrain) {
        ivector3_t voxel = space_world_to_voxel(package.ws_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];
            if (voxel_ptr->value > CHUNK_SURFACE_LEVEL) {
                package.ray_hit_terrain = 1;
                package.ws_position = package.ws_position;
                package.color = package.color;

                chunk->flags.made_modification = 1;
                chunk->flags.has_to_update_vertices = 1;

                float coeff = 0.0f;
                switch(type) {
                        
                case TT_DESTROY: {
                    coeff = -1.0f;
                } break;
                        
                case TT_BUILD: {
                    coeff = +1.0f;
                } break;
                        
                }

                float radius_squared = radius * radius;
                ivector3_t bottom_corner = voxel - ivector3_t((int32_t)radius);
                int32_t diameter = (int32_t)radius * 2 + 1;

                for (int32_t z = bottom_corner.z; z < bottom_corner.z + diameter; ++z) {
                    for (int32_t y = bottom_corner.y; y < bottom_corner.y + diameter; ++y) {
                        for (int32_t x = bottom_corner.x; x < bottom_corner.x + diameter; ++x) {
                            vector3_t current_voxel = vector3_t((float)x, (float)y, (float)z);
                            vector3_t diff = current_voxel - (vector3_t)voxel;
                            float distance_squared = glm::dot(diff, diff);

                            if (distance_squared <= radius_squared) {
                                ivector3_t current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                
                                if (current_local_coord.x < 0 || current_local_coord.x >= 16 ||
                                    current_local_coord.y < 0 || current_local_coord.y >= 16 ||
                                    current_local_coord.z < 0 || current_local_coord.z >= 16) {
                                    // If the current voxel coord is out of bounds, switch chunks
                                    ivector3_t chunk_coord = space_voxel_to_chunk(current_voxel);
                                    chunk_t *new_chunk = get_chunk(chunk_coord);

                                    chunk = new_chunk;
                                    chunk->flags.made_modification = 1;
                                    chunk->flags.has_to_update_vertices = 1;

                                    current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                }

                                uint32_t voxel_index = get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z);

                                voxel_t *voxel = &chunk->voxels[voxel_index];
                                float proportion = 1.0f - (distance_squared / radius_squared);

                                int32_t current_voxel_value = (int32_t)voxel->value;

                                int32_t new_value = (int32_t)(proportion * coeff * dt * speed) + current_voxel_value;

                                uint8_t voxel_value = 0;
                                    
                                if (new_value > (int32_t)CHUNK_MAX_VOXEL_VALUE_I) {
                                    voxel_value = (int32_t)CHUNK_MAX_VOXEL_VALUE_I;
                                }
                                else if (new_value < 0) {
                                    voxel_value = 0;
                                }
                                else {
                                    voxel_value = (uint8_t)new_value;
                                }

                                voxel->value = voxel_value;
                                voxel->color = package.color;
                            }
                        }
                    }
                }
            }
        }

        return 1;
    }

    return 0;
}

bool terraform(terraform_type_t type, terraform_package_t package, float radius, float speed, float dt) {
    if (flags.track_history) {
        return s_terraform_with_history(
            type,
            package,
            radius,
            speed,
            dt);
    }
    else {
        return s_terraform_without_history(
            type,
            package,
            radius,
            speed,
            dt);
    }
}

void reset_modification_tracker() {
    for (uint32_t i = 0; i < modified_chunk_count; ++i) {
        chunk_t *c = modified_chunks[i];
        c->flags.made_modification = 0;

        for (int32_t v = 0; v < c->history.modification_count; ++v) {
            c->history.modification_pool[c->history.modification_stack[v]] = CHUNK_SPECIAL_VALUE;
        }

        c->history.modification_count = 0;
    }

    modified_chunk_count = 0;
}

static uint8_t s_chunk_edge_voxel_value(
    int32_t x,
    int32_t y,
    int32_t z,
    bool *doesnt_exist,
    ivector3_t chunk_coord) {
    int32_t chunk_coord_offset_x = 0, chunk_coord_offset_y = 0, chunk_coord_offset_z = 0;
    int32_t final_x = x, final_y = y, final_z = z;

    if (x == CHUNK_EDGE_LENGTH) {
        final_x = 0;
        chunk_coord_offset_x = 1;
    }
    if (y == CHUNK_EDGE_LENGTH) {
        final_y = 0;
        chunk_coord_offset_y = 1;
    }
    if (z == CHUNK_EDGE_LENGTH) {
        final_z = 0;
        chunk_coord_offset_z = 1;
    }

    chunk_t *chunk_ptr = access_chunk(
        ivector3_t(chunk_coord.x + chunk_coord_offset_x,
                   chunk_coord.y + chunk_coord_offset_y,
                   chunk_coord.z + chunk_coord_offset_z));
    
    *doesnt_exist = (bool)(chunk_ptr == nullptr);
    if (*doesnt_exist) {
        return 0;
    }
    
    return chunk_ptr->voxels[get_voxel_index(final_x, final_y, final_z)].value;
}

#include "triangle_table.inc"

static const vector3_t NORMALIZED_CUBE_VERTICES[8] = {
    vector3_t(-0.5f, -0.5f, -0.5f),
    vector3_t(+0.5f, -0.5f, -0.5f),
    vector3_t(+0.5f, -0.5f, +0.5f),
    vector3_t(-0.5f, -0.5f, +0.5f),
    vector3_t(-0.5f, +0.5f, -0.5f),
    vector3_t(+0.5f, +0.5f, -0.5f),
    vector3_t(+0.5f, +0.5f, +0.5f),
    vector3_t(-0.5f, +0.5f, +0.5f) };

static void s_push_collision_vertex(
    uint8_t v0,
    uint8_t v1,
    vector3_t *vertices,
    uint8_t *voxel_values,
    uint8_t surface_level,
    collision_triangle_t *dst_triangle,
    uint32_t vertex_index) {
    float surface_level_f = (float)surface_level;
    float voxel_value0 = (float)voxel_values[v0];
    float voxel_value1 = (float)voxel_values[v1];

    if (voxel_value0 > voxel_value1) {
        float tmp = voxel_value0;
        voxel_value0 = voxel_value1;
        voxel_value1 = tmp;

        uint8_t tmp_v = v0;
        v0 = v1;
        v1 = tmp_v;
    }

    float interpolated_voxel_values = lerp(voxel_value0, voxel_value1, surface_level_f);

    vector3_t vertex = interpolate(vertices[v0], vertices[v1], interpolated_voxel_values);

    dst_triangle->vertices[vertex_index] = vertex;
}

static void s_push_collision_triangles_vertices(
    uint8_t *voxel_values,
    int32_t x,
    int32_t y,
    int32_t z,
    uint8_t surface_level,
    collision_triangle_t *dst_array,
    uint32_t *count,
    uint32_t max) {
    uint8_t bit_combination = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        bool is_over_surface = (voxel_values[i] > surface_level);
        bit_combination |= is_over_surface << i;
    }

    const int8_t *triangle_entry = &TRIANGLE_TABLE[bit_combination][0];

    uint32_t edge = 0;

    int8_t edge_pair[3] = {};

    while (triangle_entry[edge] != -1) {
        if (*count + 3 >= max) {
            break;
        }

        int8_t edge_index = triangle_entry[edge];
        edge_pair[edge % 3] = edge_index;

        if (edge % 3 == 2) {
            vector3_t vertices[8] = {};
            for (uint32_t i = 0; i < 8; ++i) {
                vertices[i] = NORMALIZED_CUBE_VERTICES[i] + vector3_t(0.5f) + vector3_t((float)x, (float)y, (float)z);
            }

            for (uint32_t i = 0; i < 3; ++i) {
                switch (edge_pair[i]) {
                case 0: { s_push_collision_vertex(0, 1, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 1: { s_push_collision_vertex(1, 2, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 2: { s_push_collision_vertex(2, 3, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 3: { s_push_collision_vertex(3, 0, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 4: { s_push_collision_vertex(4, 5, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 5: { s_push_collision_vertex(5, 6, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 6: { s_push_collision_vertex(6, 7, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 7: { s_push_collision_vertex(7, 4, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 8: { s_push_collision_vertex(0, 4, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 9: { s_push_collision_vertex(1, 5, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 10: { s_push_collision_vertex(2, 6, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                case 11: { s_push_collision_vertex(3, 7, vertices, voxel_values, surface_level, &dst_array[*count], i); } break;
                }
            }

            (*count)++;
        }

        ++edge;
    }
}

static collision_triangle_t *s_get_collision_triangles(
    uint32_t *triangle_count,
    const vector3_t &ws_center,
    const vector3_t &ws_size) {
    // Get range of triangles (v3 min - v3 max)
    ivector3_t bounding_cube_max = ivector3_t(glm::ceil(ws_center + ws_size));
    ivector3_t bounding_cube_min = ivector3_t(glm::floor(ws_center - ws_size));
    ivector3_t bounding_cube_range = bounding_cube_max - bounding_cube_min;

    bool is_between_chunks = 0;
    ivector3_t min_local_coord = space_voxel_to_local_chunk(bounding_cube_min);
    ivector3_t max_local_coord = space_voxel_to_local_chunk(bounding_cube_max);

    if (max_local_coord.x < min_local_coord.x ||
        max_local_coord.y < min_local_coord.y ||
        max_local_coord.z < min_local_coord.z) {
        is_between_chunks = 1;
    }

    uint32_t collision_vertex_count = 0;
    // Estimation
    uint32_t max_vertices = 5 * (uint32_t)glm::dot(vector3_t(bounding_cube_range), vector3_t(bounding_cube_range)) / 2;
    collision_triangle_t *triangles = LN_MALLOC(collision_triangle_t, max_vertices);

    for (int32_t z = bounding_cube_min.z; z < bounding_cube_max.z; ++z) {
        for (int32_t y = bounding_cube_min.y; y < bounding_cube_max.y; ++y) {
            for (int32_t x = bounding_cube_min.x; x < bounding_cube_max.x; ++x) {
                ivector3_t voxel_coord = ivector3_t(x, y, z);
                ivector3_t chunk_coord = space_voxel_to_chunk(voxel_coord);
                chunk_t *chunk = access_chunk(chunk_coord);

                if (chunk) {
                    bool doesnt_exist = 0;
                
                    uint8_t voxel_values[8] = {};
                    
                    ivector3_t cs_coord = space_voxel_to_local_chunk(voxel_coord);
                    
                    if (is_between_chunks) {
                        voxel_values[0] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z)].value;
                        voxel_values[1] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z, &doesnt_exist, chunk_coord);
                        voxel_values[2] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                        voxel_values[3] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                        
                        voxel_values[4] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord);
                        voxel_values[5] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord);
                        voxel_values[6] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                        voxel_values[7] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                    }
                    else {
                        voxel_values[0] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z)].value;
                        voxel_values[1] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y, cs_coord.z)].value;
                        voxel_values[2] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y, cs_coord.z + 1)].value;
                        voxel_values[3] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z + 1)].value;
                    
                        voxel_values[4] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y + 1, cs_coord.z)].value;
                        voxel_values[5] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z)].value;
                        voxel_values[6] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1)].value;
                        voxel_values[7] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y + 1, cs_coord.z + 1)].value;
                    }

                    s_push_collision_triangles_vertices(
                        voxel_values,
                        x,
                        y,
                        z,
                        CHUNK_SURFACE_LEVEL,
                        triangles,
                        &collision_vertex_count,
                        max_vertices);
                }
            }
        }
    }

    *triangle_count = collision_vertex_count;

    return triangles;
}

// This function solves the quadratic eqation "At^2 + Bt + C = 0" and is found in Kasper Fauerby's paper on collision detection and response
static bool s_get_smallest_root(
    float a,
    float b,
    float c,
    float max_r,
    float *root) {
    // Check if a solution exists
    float determinant = b * b - 4.0f * a * c;
    // If determinant is negative it means no solutions.
    if (determinant < 0.0f) return false;
    // calculate the two roots: (if determinant == 0 then
    // x1==x2 but lets disregard that slight optimization)
    float sqrt_d = sqrt(determinant);
    float r1 = (-b - sqrt_d) / (2 * a);
    float r2 = (-b + sqrt_d) / (2 * a);
    // Sort so x1 <= x2
    if (r1 > r2) {
        float temp = r2;
        r2 = r1;
        r1 = temp;
    }
    // Get lowest root:
    if (r1 > 0 && r1 < max_r) {
        *root = r1;
        return true;
    }
    // It is possible that we want x2 - this can happen
    // if x1 < 0
    if (r2 > 0 && r2 < max_r) {
        *root = r2;
        return true;
    }

    // No (valid) solutions
    return false;
}


static float s_get_plane_constant(
    const vector3_t &plane_point,
    const vector3_t &plane_normal) {
    return -glm::dot(plane_point, plane_normal);
}

static bool s_facing_triangle(
    const vector3_t &es_normalised_velocity,
    const vector3_t &es_normal) {
    return (glm::dot(es_normalised_velocity, es_normal) <= 0.0f);
}

static bool s_inside_triangle(
    const vector3_t &plane_contact_point,
    collision_triangle_t *triangle) {
    vector3_t cross0 = glm::cross(triangle->v.c - triangle->v.b, plane_contact_point - triangle->v.b);
    vector3_t cross1 = glm::cross(triangle->v.c - triangle->v.b, triangle->v.a - triangle->v.b);

    if (glm::dot(cross0, cross1) >= 0.0f) {
        cross0 = glm::cross(triangle->v.c - triangle->v.a, plane_contact_point - triangle->v.a);
        cross1 = glm::cross(triangle->v.c - triangle->v.a, triangle->v.b - triangle->v.a);

        if (glm::dot(cross0, cross1) >= 0.0f) {
            cross0 = glm::cross(triangle->v.b - triangle->v.a, plane_contact_point - triangle->v.a);
            cross1 = glm::cross(triangle->v.b - triangle->v.a, triangle->v.c - triangle->v.a);

            if (glm::dot(cross0, cross1) >= 0.0f) {
                return true;
            }
        }
    }

    return false;
}

static bool s_touched_vertex(
    float a,
    float *cinstance0,
    const vector3_t &vertex,
    terrain_collision_t *collision) {
    float b = 2.0f * glm::dot(collision->es_velocity, collision->es_position - vertex);
    float c = glm::dot(vertex - collision->es_position, vertex - collision->es_position) - 1.0f;

    float new_cinstance;
    
    if (s_get_smallest_root(a, b, c, *cinstance0, &new_cinstance)) {
        *cinstance0 = new_cinstance;
        return true;
    }

    return false;
}

static bool s_touched_edge(
    float *cinstance0,
    vector3_t *new_position,
    const vector3_t &vertex0,
    const vector3_t &vertex1,
    terrain_collision_t *collision) {
    vector3_t edge_vector = vertex1 - vertex0;
    vector3_t position_to_vertex0 = vertex0 - collision->es_position;
    float velocity_length2 = glm::dot(collision->es_velocity, collision->es_velocity);
    float edge_length2 = glm::dot(edge_vector, edge_vector);
    float edge_dot_velocity = glm::dot(edge_vector, collision->es_velocity);
    float edge_dot_position_to_vertex0 = glm::dot(edge_vector, position_to_vertex0);
    float position_to_vertex0_length2 = glm::dot(position_to_vertex0, position_to_vertex0);

    float a = edge_length2 * (-velocity_length2) + (edge_dot_velocity * edge_dot_velocity);
    float b = edge_length2 * (2.0f * glm::dot(collision->es_velocity, position_to_vertex0)) - (2.0f * edge_dot_velocity * edge_dot_position_to_vertex0);
    float c = edge_length2 * (1.0f - position_to_vertex0_length2) + (edge_dot_position_to_vertex0 * edge_dot_position_to_vertex0);

    float new_cinstance;
    
    if (s_get_smallest_root(a, b, c, *cinstance0, &new_cinstance)) {
        // Where on the line did the collision happen (did it even happen on the edge, or just on the infinite line of the edge)
        float proportion = (edge_dot_velocity * new_cinstance - edge_dot_position_to_vertex0) / edge_length2;
        if (proportion >= 0.0f && proportion <= 1.0f) {
            *cinstance0 = new_cinstance;
            *new_position = vertex0 + proportion * edge_vector;
            return true;
        }
    }

    return false;
}

static bool s_collided_with_triangle(
    terrain_collision_t *collision,
    collision_triangle_t *es_triangle) {
    vector3_t es_a = es_triangle->v.a;
    vector3_t es_b = es_triangle->v.b;
    vector3_t es_c = es_triangle->v.c;

    vector3_t es_plane_normal = glm::normalize(glm::cross(es_b - es_a, es_c - es_a));

    float es_distance_to_plane = 0.0f;

    bool inside_terrain = 0;
    
    if (s_facing_triangle(collision->es_normalised_velocity, es_plane_normal)) {
        // get plane constant
        float plane_constant = s_get_plane_constant(es_a, es_plane_normal);
        es_distance_to_plane = glm::dot(collision->es_position, es_plane_normal) + plane_constant;
        float plane_normal_dot_velocity = glm::dot(es_plane_normal, collision->es_velocity);

        bool sphere_inside_plane = 0;

        float cinstance0 = 0.0f, cinstance1 = 0.0f;
        
        if (plane_normal_dot_velocity == 0.0f) {
            // sphere velocity is parallel to plane surface (& facing plane, as we calculated before)
            if (glm::abs(es_distance_to_plane) >= 1.0f) {
                // sphere is not in plane and distance is more than sphere radius, cannot possibly collide
                return false;
            }
            else {
                sphere_inside_plane = 1;
            }
        }
        else {
            // sphere velocity is not parallel to plane surface (& facing towards plane)
            // need to check collision with triangle surface, edges and vertices
            // collision instance 0 (first time sphere touches plane)
            cinstance0 = (1.0f - es_distance_to_plane) / plane_normal_dot_velocity;
            cinstance1 = (-1.0f - es_distance_to_plane) / plane_normal_dot_velocity;

            // The sphere is inside the fricking plane
            if (cinstance0 < 0.0f) {
                inside_terrain = 1;
                //LOG_ERROR("There is problem: sphere is inside the terrain\n");
            }

            // always make sure that 0 corresponds to closer collision and 1 responds to further
            if (cinstance0 > cinstance1) {
                float t = cinstance0;
                cinstance0 = cinstance1;
                cinstance1 = t;
            }

            if (cinstance0 > 1.0f || cinstance1 < 0.0f) {
                // either triangle plane is behind, or too far (0.0f to 1.0f represents 0.0f to dt, if we cinstance0 > 1.0f, we are trying to go further than we can in this timeframe)
                return false;
            }

            if (cinstance0 < 0.0f) cinstance0 = 0.0f;
            if (cinstance1 > 1.0f) cinstance1 = 1.0f;
        }

        // point where sphere intersects with plane, not triangle
        vector3_t plane_contact_point = (collision->es_position + cinstance0 * collision->es_velocity - es_plane_normal);

        bool detected_collision = 0;
        float cinstance = 1.0f;
        vector3_t triangle_contact_point;

        if (!sphere_inside_plane) {
            if (s_inside_triangle(plane_contact_point, es_triangle)) {
                // sphere collided with triangle
                detected_collision = 1;
                cinstance = cinstance0;

                if (inside_terrain) {
                    
                    //LOG_ERRORV("We got a problem, cinstance < 0: %f\n", cinstance);
                }

                if (cinstance < 0.0f) {
                }
                
                triangle_contact_point = plane_contact_point;
            }
            else {
                inside_terrain = 0;
            }
        }

        // check triangle edges / vertices
        if (!detected_collision) {
            float a; /*, b, c*/

            a = glm::dot(collision->es_velocity, collision->es_velocity);

            if (s_touched_vertex(a, &cinstance, es_triangle->v.a, collision)) {
                detected_collision = true;
                triangle_contact_point = es_triangle->v.a;
            }

            if (s_touched_vertex(a, &cinstance, es_triangle->v.b, collision)) {
                detected_collision = true;
                triangle_contact_point = es_triangle->v.b;
            }

            if (s_touched_vertex(a, &cinstance, es_triangle->v.c, collision)) {
                detected_collision = true;
                triangle_contact_point = es_triangle->v.c;
            }

            vector3_t new_position_on_edge;

            if (s_touched_edge(&cinstance, &new_position_on_edge, es_a, es_b, collision)) {
                detected_collision = true;
                triangle_contact_point = new_position_on_edge;
            }

            if (s_touched_edge(&cinstance, &new_position_on_edge, es_b, es_c, collision)) {
                detected_collision = true;
                triangle_contact_point = new_position_on_edge;
            }

            if (s_touched_edge(&cinstance, &new_position_on_edge, es_c, es_a, collision)) {
                detected_collision = true;
                triangle_contact_point = new_position_on_edge;
            }
        }

        if (detected_collision) {
            float distance_to_collision = glm::abs(cinstance) * glm::length(collision->es_velocity);

            if (!collision->detected || distance_to_collision < collision->es_nearest_distance) {
                if (inside_terrain) {
                    collision->es_nearest_distance = es_distance_to_plane;
                }
                else {
                    collision->es_nearest_distance = distance_to_collision;
                }
                
                collision->es_contact_point = triangle_contact_point;
                collision->es_surface_normal = es_plane_normal;
                collision->detected = 1;
                collision->under_terrain = inside_terrain;

                collision->has_detected_previously = 1;

                return true;
            }
        }
    }

    return false;
}

vector3_t collide_and_slide(
    terrain_collision_t *collision) {
    // Get intersecting triangles
    uint32_t triangle_count = 0;
    collision_triangle_t *triangles = s_get_collision_triangles(
        &triangle_count,
        collision->ws_position,
        collision->ws_size);

    if (collision->recurse > 5) {
        return collision->es_position;
    }
    
    // Avoid division by zero
    if (glm::abs(glm::dot(collision->es_velocity, collision->es_velocity)) == 0.0f) {
        collision->es_normalised_velocity = vector3_t(0.0f);
    }
    else {
        collision->es_normalised_velocity = glm::normalize(collision->es_velocity);
    }

    collision->has_detected_previously = 0;

    for (uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        collision_triangle_t *triangle = &triangles[triangle_index];

        for (uint32_t i = 0; i < 3; ++i) {
            triangle->vertices[i] /= collision->ws_size;
        }

        // Check collision with this triangle (now is ellipsoid space)
        s_collided_with_triangle(collision, triangle);
    }

    if (!collision->has_detected_previously) {
        // No more collisions, just return position + velocity
        return collision->es_position + collision->es_velocity;
    }

    // Point where sphere would travel to if there was no collisions
    // No-collision-destination
    vector3_t noc_destination = collision->es_position + collision->es_velocity;
    vector3_t actual_position = collision->es_position;

    float close_distance = 0.005f;

    if (collision->under_terrain) {
        //actual_position = collision->es_contact_point + collision->es_surface_normal * (1.0f + close_distance);
        actual_position = collision->es_position + (1.0f + close_distance - collision->es_nearest_distance) * collision->es_surface_normal;

        ++(collision->recurse);
        collision->es_position = actual_position;
        collision->es_nearest_distance = 1000.0f;

        return collide_and_slide(collision);
    }
    else {
        if (collision->es_nearest_distance >= close_distance) {
            // Make sure that sphere never touches the terrain
            vector3_t normalized_velocity = glm::normalize(collision->es_velocity);
            vector3_t velocity = normalized_velocity * (collision->es_nearest_distance - close_distance);

            actual_position = collision->es_position + velocity;

            collision->es_contact_point -= close_distance * normalized_velocity;
        }

        vector3_t plane_origin = collision->es_contact_point;
        vector3_t plane_normal = glm::normalize(actual_position - collision->es_contact_point);

        float plane_constant = s_get_plane_constant(plane_origin, plane_normal);
        float distance_noc_dest_to_plane = glm::dot(noc_destination, plane_normal) + plane_constant;

        vector3_t plane_destination_point = noc_destination - distance_noc_dest_to_plane * plane_normal;
        vector3_t actual_velocity = plane_destination_point - collision->es_contact_point;

        float distance_to_plane = glm::dot(actual_position, plane_normal) + plane_constant;
        if (distance_to_plane < 1.0f) {
            // Make sure that sphere is not inside the plane
            //actual_position += (1.0f - distance_to_plane) * plane_normal;
            //LOG_ERROR("Sinked into terrain\n");
        }
    
        ++(collision->recurse);
        collision->es_position = actual_position;
        collision->es_velocity = actual_velocity;
        // TODO: Make sure to check that it's plane_normal and not triangle surface normal
        collision->es_surface_normal = plane_normal;
    
        if (glm::dot(actual_velocity, actual_velocity) < close_distance * close_distance) {
            return actual_position;
        }

        return collide_and_slide(collision);
    }
}
