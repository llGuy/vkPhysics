#include "w_internal.hpp"
#include <common/math.hpp>
#include <common/allocators.hpp>

struct terrain_collision_t {
    // Data about collision
};

struct collision_triangle_t {
    union {
        struct {
            vector3_t a;
            vector3_t b;
            vector3_t c;
        } v;
        vector3_t vertices[3];
    };
};

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
    
    return chunk_ptr->voxels[get_voxel_index(final_x, final_y, final_z)];
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
    ivector3_t bounding_cube_min = ivector3_t(glm::ceil(ws_center - ws_size));
    ivector3_t bounding_cube_range = bounding_cube_max - bounding_cube_min;

    bool is_between_chunks = 0;
    ivector3_t min_local_coord = w_convert_voxel_to_local_chunk(bounding_cube_min);
    ivector3_t max_local_coord = w_convert_voxel_to_local_chunk(bounding_cube_max);

    if (max_local_coord.x < min_local_coord.x ||
        max_local_coord.y < min_local_coord.y ||
        max_local_coord.z < min_local_coord.z) {
        is_between_chunks = 1;
    }

    uint32_t collision_vertex_count;
    // Estimation
    uint32_t max_vertices = 5 * (uint32_t)glm::dot(vector3_t(bounding_cube_range), vector3_t(bounding_cube_range)) / 2;
    collision_triangle_t *triangles = LN_MALLOC(collision_triangle_t, max_vertices);

    for (int32_t z = bounding_cube_min.z; z < bounding_cube_max.z; ++z) {
        for (int32_t y = bounding_cube_min.y; y < bounding_cube_max.y; ++y) {
            for (int32_t x = bounding_cube_min.x; x < bounding_cube_max.x; ++x) {
                ivector3_t voxel_coord = ivector3_t(x, y, z);
                ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_coord);
                chunk_t *chunk = get_chunk(chunk_coord);

                bool doesnt_exist = 0;

                uint8_t voxel_values[8] = {};

                ivector3_t cs_coord = w_convert_voxel_to_local_chunk(voxel_coord);

                if (is_between_chunks) {
                    voxel_values[0] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z)];
                    voxel_values[1] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z, &doesnt_exist, chunk_coord);
                    voxel_values[2] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                    voxel_values[3] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                    
                    voxel_values[4] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord);
                    voxel_values[5] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord);
                    voxel_values[6] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                    voxel_values[7] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord);
                }
                else {
                    voxel_values[0] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z)];
                    voxel_values[1] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y, cs_coord.z)];
                    voxel_values[2] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y, cs_coord.z + 1)];
                    voxel_values[3] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z + 1)];
                    
                    voxel_values[4] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y + 1, cs_coord.z)];
                    voxel_values[5] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z)];
                    voxel_values[6] = chunk->voxels[get_voxel_index(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1)];
                    voxel_values[7] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y + 1, cs_coord.z + 1)];
                }

                s_push_collision_triangles_vertices(
                    voxel_values,
                    x,
                    y,
                    z,
                    get_surface_level(),
                    triangles,
                    &collision_vertex_count,
                    max_vertices);
            }
        }
    }
}

terrain_collision_t collide_and_slide(
    const vector3_t &ws_size,
    const vector3_t &ws_position,
    vector3_t *ws_velocity,
    uint32_t recurse_depth = 0) {
    if (recurse_depth > 5) {
        return terrain_collision_t{};
    }
    
    // Get intersecting triangles
    uint32_t triangle_count = 0;
    collision_triangle_t *triangles = s_get_collision_triangles(
        &triangle_count,
        ws_position,
        ws_size);
}
