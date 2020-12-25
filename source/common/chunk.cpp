#include "math.hpp"
#include "game.hpp"
#include "chunk.hpp"
#include "constant.hpp"
#include "containers.hpp"

template <typename T>
static void s_iterate_3d(
    const ivector3_t &center,
    uint32_t radius,
    T to_apply) {
    int32_t start_z = center.z - (int32_t)radius;
    int32_t start_y = center.y - (int32_t)radius;
    int32_t start_x = center.x - (int32_t)radius;

    int32_t diameter = (int32_t)radius * 2 + 1;

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                to_apply(x, y, z);
            }
        }
    }
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

    chunk_t *current_chunk = g_game->get_chunk(
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
                    if (chunk_origin_diff.x >= 0 && chunk_origin_diff.x < 16 &&
                        chunk_origin_diff.y >= 0 && chunk_origin_diff.y < 16 &&
                        chunk_origin_diff.z >= 0 && chunk_origin_diff.z < 16) {
                        // Is within current chunk boundaries
                        float proportion = generation_proc(distance_squared, smaller_radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

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
                        current_chunk = g_game->get_chunk(c);
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

    chunk_t *current_chunk = g_game->get_chunk(
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
                        current_chunk = g_game->get_chunk(c);
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
            chunk_t *chunk = g_game->get_chunk(chunk_coord);
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
                    chunk_t *chunk = g_game->get_chunk(chunk_coord);
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
    float max_reach,
    voxel_color_t color) {
    vector3_t vs_position = ws_ray_start;
    vector3_t vs_dir = ws_ray_direction;

    static const float PRECISION = 1.0f / 15.0f;
    
    vector3_t vs_step = vs_dir * max_reach * PRECISION;

    float max_reach_squared = max_reach * max_reach;

    terraform_package_t package = {};
    package.ray_hit_terrain = 0;
    package.color = color;

    for (; glm::dot(vs_position - ws_ray_start, vs_position - ws_ray_start) < max_reach_squared; vs_position += vs_step) {
        ivector3_t voxel = space_world_to_voxel(vs_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = g_game->access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];

            terrain_collision_t collision = {};
            collision.ws_size = vector3_t(0.1f);
            collision.ws_position = vs_position;
            collision.ws_velocity = vs_dir;
            collision.es_position = collision.ws_position / collision.ws_size;
            collision.es_velocity = collision.ws_velocity / collision.ws_size;

            check_ray_terrain_collision(&collision);

            if (collision.detected) {
                package.ray_hit_terrain = 1;
                package.ws_contact_point = collision.es_contact_point * collision.ws_size;
                package.ws_position = glm::round(package.ws_contact_point);
                break;
            }
        }
    }

    return package;
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
        chunk_t *chunk = g_game->access_chunk(chunk_coord);

        if (!chunk->flags.made_modification) {
            // Push this chunk onto list of modified chunks
            g_game->modified_chunks[g_game->modified_chunk_count++] = chunk;
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
                            chunk_t *new_chunk = g_game->get_chunk(chunk_coord);

                            chunk = new_chunk;

                            if (!chunk->flags.made_modification) {
                                // Push this chunk onto list of modified chunks
                                g_game->modified_chunks[g_game->modified_chunk_count++] = chunk;
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
        chunk_t *chunk = g_game->access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];
            if (voxel_ptr->value > CHUNK_SURFACE_LEVEL) {
                package.ray_hit_terrain = 1;

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
                                    chunk_t *new_chunk = g_game->get_chunk(chunk_coord);

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
    if (g_game->flags.track_history) {
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

