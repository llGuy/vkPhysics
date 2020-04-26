#include "w_internal.hpp"
#include <common/math.hpp>
#include <common/allocators.hpp>

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
    vector3_t *dst_array,
    uint32_t *count) {
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

    dst_array[(*count)++] = vertex;
}

static void s_push_collision_triangles_vertices(
    uint8_t *voxel_values,
    int32_t x,
    int32_t y,
    int32_t z,
    uint8_t surface_level,
    vector3_t *dst_array,
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
                case 0: { s_push_collision_vertex(0, 1, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 1: { s_push_collision_vertex(1, 2, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 2: { s_push_collision_vertex(2, 3, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 3: { s_push_collision_vertex(3, 0, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 4: { s_push_collision_vertex(4, 5, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 5: { s_push_collision_vertex(5, 6, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 6: { s_push_collision_vertex(6, 7, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 7: { s_push_collision_vertex(7, 4, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 8: { s_push_collision_vertex(0, 4, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 9: { s_push_collision_vertex(1, 5, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 10: { s_push_collision_vertex(2, 6, vertices, voxel_values, surface_level, dst_array, count); } break;
                case 11: { s_push_collision_vertex(3, 7, vertices, voxel_values, surface_level, dst_array, count); } break;
                }
            }
        }

        ++edge;
    }
}

static vector3_t *s_get_collision_triangles(
    const vector3_t &es_center,
    const vector3_t &ws_size,
    uint32_t *triangle_vertex_count) {
    vector3_t ws_center = es_center * ws_size;
    
    ivector3_t bounding_cube_max = ivector3_t(glm::ceil(ws_center + ws_size));
    ivector3_t bounding_cube_min = ivector3_t(glm::floor(ws_center - ws_size));
    ivector3_t bounding_cube_range = bounding_cube_max - bounding_cube_min;

    bool is_between_chunks = 0;
    ivector3_t min_voxel_coord = w_convert_voxel_to_local_chunk(bounding_cube_min);
    ivector3_t max_voxel_coord = min_voxel_coord + bounding_cube_range;

    if (max_voxel_coord.x < min_voxel_coord.x ||
        max_voxel_coord.y < min_voxel_coord.y ||
        max_voxel_coord.z < min_voxel_coord.z ||
        max_voxel_coord.x >= 15 ||
        max_voxel_coord.y >= 15 ||
        max_voxel_coord.z >= 15) {
        is_between_chunks = 1;
    }

    uint32_t collision_vertex_count = 0;
    uint32_t max_vertices = 3 * 5 * (uint32_t)glm::dot(vector3_t(bounding_cube_range), vector3_t(bounding_cube_range)) / 2;
    vector3_t *triangle_vertices = (vector3_t *)LN_MALLOC(vector3_t, max_vertices);

    for (int32_t z = bounding_cube_min.z; z < bounding_cube_max.z; ++z) {
        for (int32_t y = bounding_cube_min.y; y < bounding_cube_max.y; ++y) {
            for (int32_t x = bounding_cube_min.x; x < bounding_cube_max.x; ++x) {
                ivector3_t voxel_pair_origin = ivector3_t(x, y, z);
                ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_pair_origin);
                chunk_t *chunk = get_chunk(chunk_coord);

                bool doesnt_exist = 0;

                uint8_t voxel_values[8] = {};

                ivector3_t cs_coord = w_convert_voxel_to_local_chunk(ivector3_t(x, y, z));
                
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
                    triangle_vertices,
                    &collision_vertex_count,
                    max_vertices);
            }
        }
    }

    *triangle_vertex_count = collision_vertex_count;

    return triangle_vertices;
}

static bool s_is_point_in_triangle(const vector3_t &point, const vector3_t &tri_point_a, const vector3_t &tri_point_b, const vector3_t &tri_point_c) {
    vector3_t cross11 = glm::cross((tri_point_c - tri_point_b), (point - tri_point_b));
    vector3_t cross12 = glm::cross((tri_point_c - tri_point_b), (tri_point_a - tri_point_b));
    float d1 = glm::dot(cross11, cross12);
    if (d1 >= 0) {
        vector3_t cross21 = glm::cross((tri_point_c - tri_point_a), (point - tri_point_a));
        vector3_t cross22 = glm::cross((tri_point_c - tri_point_a), (tri_point_b - tri_point_a));
        float d2 = glm::dot(cross21, cross22);
        if (d2 >= 0) {
            vector3_t cross31 = glm::cross((tri_point_b - tri_point_a), (point - tri_point_a));
            vector3_t cross32 = glm::cross((tri_point_b - tri_point_a), (tri_point_c - tri_point_a));
            float d3 = glm::dot(cross31, cross32);
            if (d3 >= 0) {
                return 1;
            }
        }
    }
    return 0;
}


// This function solves the quadratic eqation "At^2 + Bt + C = 0" and is found in Kasper Fauerby's paper on collision detection and response
static bool s_get_smallest_root(float a, float b, float c, float max_r, float *root) {
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


static float s_get_plane_constant(const vector3_t &plane_point, const vector3_t &plane_normal) {
    return -((plane_point.x * plane_normal.x) + (plane_point.y * plane_normal.y) + (plane_point.z * plane_normal.z));
}



static void s_check_collision_with_vertex(
    const vector3_t &es_sphere_velocity,
    const vector3_t &es_sphere_position,
    const vector3_t &es_vertex,
    const vector3_t &es_surface_normal,
    terrain_collision_t *collision) {
    float a = distance_squared(es_sphere_velocity);
    float b = 2.0f * glm::dot(es_sphere_velocity, es_sphere_position - es_vertex);
    float c = distance_squared(es_vertex - es_sphere_position) - 1.0f;

    float new_resting_instance;
    if (s_get_smallest_root(a, b, c, 1.0f, &new_resting_instance)) {
        // TODO: Make sure that we are not using glm::length
        float es_distance_to_center = glm::length(new_resting_instance * es_sphere_velocity);

        if (es_distance_to_center < collision->es_distance_to_center) {
            collision->detected = 1;
            collision->primitive_type = collision_primitive_type_t::CPT_VERTEX;
            collision->es_distance_to_center = es_distance_to_center;
            collision->es_contact_point = es_vertex;
            collision->es_surface_normal = es_surface_normal;
        }
    }
}

static void s_check_collision_with_edge(
    const vector3_t &es_sphere_velocity,
    const vector3_t &es_sphere_position,
    const vector3_t &es_vertex_a,
    const vector3_t &es_vertex_b,
    const vector3_t &es_surface_normal,
    terrain_collision_t *collision) {
    vector3_t es_edge_diff = es_vertex_b - es_vertex_a;
    vector3_t es_sphere_pos_to_vertex = es_vertex_a - es_sphere_position;

    float a = distance_squared(es_edge_diff) * -distance_squared(es_sphere_velocity) + squared(glm::dot(es_edge_diff, es_sphere_velocity));
    float b = distance_squared(es_edge_diff) * 2.0f * glm::dot(es_sphere_velocity, es_sphere_pos_to_vertex) - 2.0f * (glm::dot(es_edge_diff, es_sphere_velocity) * glm::dot(es_edge_diff, es_sphere_pos_to_vertex));
    float c = distance_squared(es_edge_diff) * (1.0f - distance_squared(es_sphere_pos_to_vertex)) + squared(glm::dot(es_edge_diff, es_sphere_pos_to_vertex));

    float new_resting_instance;
    if (s_get_smallest_root(a, b, c, 1.0f, &new_resting_instance)) {
        float in_edge_proportion = (glm::dot(es_edge_diff, es_sphere_velocity) * new_resting_instance - glm::dot(es_edge_diff, es_sphere_pos_to_vertex)) / distance_squared(es_edge_diff);

        if (in_edge_proportion >= 0.0f && in_edge_proportion <= 1.0f) {
            vector3_t es_sphere_contact_point = es_vertex_a + in_edge_proportion * es_edge_diff;
            // TODO: Get rid of glm::length
            float es_distance_to_center = glm::length(new_resting_instance * es_sphere_velocity);

            if (es_distance_to_center < collision->es_distance_to_center) {
                collision->detected = 1;
                collision->primitive_type = collision_primitive_type_t::CPT_EDGE;
                collision->es_distance_to_center = es_distance_to_center;
                collision->es_contact_point = es_sphere_contact_point;
                collision->es_surface_normal = es_surface_normal;
            }
        }
    }
}


static vector3_t s_adjust_player_position_to_not_be_under_terrain(
    vector3_t *triangle_vertices,
    vector3_t es_center,
    const vector3_t &es_velocity,
    terrain_collision_t *closest,
    const vector3_t &size) {
    bool under_terrain = 1;
    while (under_terrain) {
        bool found_collision = 0;
        float first_resting_instance;

        vector3_t es_fa = triangle_vertices[0];
        vector3_t es_fb = triangle_vertices[1];
        vector3_t es_fc = triangle_vertices[2];

        vector3_t es_up_normal_of_triangle = glm::normalize(glm::cross(es_fb - es_fa, es_fc - es_fa));

        float velocity_dot_normal = glm::dot(glm::normalize(es_velocity), es_up_normal_of_triangle);

        if (velocity_dot_normal > 0.0f) {
            return es_center;
        }

        float plane_constant = -((es_fa.x * es_up_normal_of_triangle.x) + (es_fa.y * es_up_normal_of_triangle.y) + (es_fa.z * es_up_normal_of_triangle.z));

        bool must_check_only_for_edges_and_vertices = 0;
        float normal_dot_velocity = glm::dot(es_velocity, es_up_normal_of_triangle);
        float sphere_plane_distance = glm::dot(es_center, es_up_normal_of_triangle) + plane_constant;

        if (normal_dot_velocity == 0.0f) {
            if (glm::abs(sphere_plane_distance) >= 1.0f) {
                return es_center;
            }
            else {
                must_check_only_for_edges_and_vertices = 1;
            }
        }

        // Check collision with triangle face
        first_resting_instance = (1.0f - sphere_plane_distance) / normal_dot_velocity;
        float second_resting_instance = (-1.0f - sphere_plane_distance) / normal_dot_velocity;

        if (first_resting_instance > second_resting_instance) {
            float f = first_resting_instance;
            first_resting_instance = second_resting_instance;
            second_resting_instance = f;
        }
        if (first_resting_instance > 1.0f || second_resting_instance < 0.0f) {
            return es_center;
        }
        if (first_resting_instance < 0.0f) first_resting_instance = 0.0f;
        if (second_resting_instance < 1.0f) second_resting_instance = 1.0f;

        vector3_t es_contact_point = es_center + (first_resting_instance * es_velocity) - es_up_normal_of_triangle;

        if (s_is_point_in_triangle(es_contact_point, es_fa, es_fb, es_fc)) {
            float sphere_point_plane_distance = glm::dot(es_center - es_up_normal_of_triangle, es_up_normal_of_triangle) + plane_constant;
            if (sphere_point_plane_distance < 0.0f) {
                if (sphere_point_plane_distance > -0.00001f) {
                    // Do some sort of recursive loop until the player is out of the terrain
                    sphere_point_plane_distance = -0.5f;
                }
                // Adjust the sphere position, and call the function
                es_center = es_center - es_up_normal_of_triangle * sphere_point_plane_distance;

                under_terrain = 1;
            }
            else {
                under_terrain = 0;
            }
        }
    }

    return es_center;
}

static void s_collide_with_triangle(
    vector3_t *triangle_vertices,
    const vector3_t &es_center,
    const vector3_t &es_velocity,
    terrain_collision_t *closest,
    const vector3_t &ws_size) {
    bool found_collision = 0;
    float first_resting_instance;

    vector3_t es_fa = triangle_vertices[0];
    vector3_t es_fb = triangle_vertices[1];
    vector3_t es_fc = triangle_vertices[2];

    vector3_t es_up_normal_of_triangle = glm::normalize(glm::cross(es_fb - es_fa, es_fc - es_fa));

    float velocity_dot_normal = glm::dot(glm::normalize(es_velocity), es_up_normal_of_triangle);

    if (velocity_dot_normal > 0.0f) {
        return;
    }
    
    float plane_constant = -((es_fa.x * es_up_normal_of_triangle.x) + (es_fa.y * es_up_normal_of_triangle.y) + (es_fa.z * es_up_normal_of_triangle.z));

    bool must_check_only_for_edges_and_vertices = 0;
    float normal_dot_velocity = glm::dot(es_velocity, es_up_normal_of_triangle);
    float sphere_plane_distance = glm::dot(es_center, es_up_normal_of_triangle) + plane_constant;

    if (normal_dot_velocity == 0.0f) {
        if (glm::abs(sphere_plane_distance) >= 1.0f) {
            return;
        }
        else {
            must_check_only_for_edges_and_vertices = 1;
        }
    }

    if (!must_check_only_for_edges_and_vertices) {
        // Check collision with triangle face
        first_resting_instance = (1.0f - sphere_plane_distance) / normal_dot_velocity;
        float second_resting_instance = (-1.0f - sphere_plane_distance) / normal_dot_velocity;

        if (first_resting_instance > second_resting_instance) {
            float f = first_resting_instance;
            first_resting_instance = second_resting_instance;
            second_resting_instance = f;
        }
        if (first_resting_instance > 1.0f || second_resting_instance < 0.0f) {
            return;
        }
        if (first_resting_instance < 0.0f) first_resting_instance = 0.0f;
        if (second_resting_instance < 1.0f) second_resting_instance = 1.0f;

        vector3_t es_contact_point = es_center + (first_resting_instance * es_velocity) - es_up_normal_of_triangle;

        if (s_is_point_in_triangle(es_contact_point, es_fa, es_fb, es_fc)) {
            // TODO: Get rid of glm::length
            float es_distance_to_center = glm::length(es_velocity * first_resting_instance);
            if (es_distance_to_center < closest->es_distance_to_center) {
                float sphere_point_plane_distance = glm::dot(es_center - es_up_normal_of_triangle, es_up_normal_of_triangle) + plane_constant;
                if (sphere_point_plane_distance < 0.0f && !closest->under_terrain) {
                    if (sphere_point_plane_distance > -0.00001f) {
                        // Do some sort of recursive loop until the player is out of the terrain
                        sphere_point_plane_distance = -0.01f;
                    }
                    // Adjust the sphere position, and call the function
                    vector3_t es_new_sphere_position = es_center - es_up_normal_of_triangle * sphere_point_plane_distance;

                    closest->under_terrain = 1;
                    //closest->es_collision_point = adjust_player_position_to_not_be_under_terrain(triangle_vertices, es_new_sphere_position, es_velocity, closest, size);
                    closest->es_collision_point = es_new_sphere_position;
                    closest->es_surface_normal = es_up_normal_of_triangle;
                    closest->es_distance_from_triangle = sphere_point_plane_distance;

                    return;
                }

                found_collision = 1;
                closest->detected = 1;
                closest->primitive_type = collision_primitive_type_t::CPT_FACE;
                closest->es_distance_to_center = es_distance_to_center;
                closest->es_contact_point = es_contact_point;
                closest->es_surface_normal = es_up_normal_of_triangle;
            }
        }
    }

    if (!found_collision) {
        // Check collision with vertices
        s_check_collision_with_vertex(es_velocity, es_center, es_fa, es_up_normal_of_triangle, closest);
        s_check_collision_with_vertex(es_velocity, es_center, es_fb, es_up_normal_of_triangle, closest);
        s_check_collision_with_vertex(es_velocity, es_center, es_fc, es_up_normal_of_triangle, closest);

        // Check collision with edges
        s_check_collision_with_edge(es_velocity, es_center, es_fa, es_fb, es_up_normal_of_triangle, closest);
        s_check_collision_with_edge(es_velocity, es_center, es_fb, es_fc, es_up_normal_of_triangle, closest);
        s_check_collision_with_edge(es_velocity, es_center, es_fc, es_fa, es_up_normal_of_triangle, closest);
    }
}

// Just detects collision
terrain_collision_t collide_and_slide(
    const vector3_t &es_center,
    const vector3_t &es_velocity,
    const vector3_t &ws_size,
    uint32_t recurse_depth,
    terrain_collision_t previous_collision,
    terrain_collision_t *first_collision) {
    uint32_t collision_vertex_count = 0;
    vector3_t *triangle_vertices = s_get_collision_triangles(
        es_center,
        ws_size,
        &collision_vertex_count);

    terrain_collision_t closest_collision = {};
    // Set this to high value so that any *first* vertex will override this
    closest_collision.es_distance_to_center = 1000.0f;
    
    for (uint32_t triangle = 0; triangle < collision_vertex_count / 3; ++triangle) {
        vector3_t *triangle_ptr = &(triangle_vertices[triangle * 3]);

        // Convert from xs to es (ellipsoid space) - triangle vertices are already in world space
        for (uint32_t i = 0; i < 3; ++i) {
            triangle_ptr[i] /= ws_size;
        }

        s_collide_with_triangle(triangle_ptr, es_center, es_velocity, &closest_collision, ws_size);
    }

    const float es_very_close_distance_from_terrain = .01f;

    if (closest_collision.under_terrain) {
        terrain_collision_t collision = {};
        collision.detected = 1;
        collision.es_collision_point = closest_collision.es_collision_point;
        collision.es_surface_normal = closest_collision.es_surface_normal;
        collision.under_terrain = 1;
        return(collision);
    }
    else if (closest_collision.detected) {
        uint32_t max_recursion_depth = 5;

        // TODO: Do not calculate the ellipsoid space of these values again, just do it once at the beginning of the function
        vector3_t es_sphere_position = es_center;
        vector3_t es_sphere_velocity = es_velocity;

        vector3_t es_new_sphere_position = es_sphere_position;
        vector3_t es_sphere_destination_point = es_sphere_position + es_sphere_velocity;
            
        if (closest_collision.es_distance_to_center >= es_very_close_distance_from_terrain) {
            vector3_t es_surface_normalized_velocity = glm::normalize(es_sphere_velocity);
            vector3_t es_scaled_velocity = es_surface_normalized_velocity * (closest_collision.es_distance_to_center - es_very_close_distance_from_terrain);
            es_new_sphere_position = es_sphere_position + es_scaled_velocity;

            closest_collision.es_contact_point -= es_very_close_distance_from_terrain * es_surface_normalized_velocity;
        }

        // Get slide plane information
        vector3_t es_slide_plane_point = closest_collision.es_contact_point;
        vector3_t es_slide_plane_normal = glm::normalize(es_new_sphere_position - closest_collision.es_contact_point);

        float plane_constant = s_get_plane_constant(es_slide_plane_point, es_slide_plane_normal);
        float dest_point_dist_from_plane = glm::dot(es_sphere_destination_point, es_slide_plane_normal) + plane_constant;

        vector3_t es_new_sphere_destination_point = es_sphere_destination_point - dest_point_dist_from_plane * es_slide_plane_normal;
        vector3_t es_new_velocity = es_new_sphere_destination_point - closest_collision.es_contact_point;

        float new_velocity_distance_squared = distance_squared(es_new_velocity);
        float very_close_distance_squared = squared(es_very_close_distance_from_terrain);

        if (new_velocity_distance_squared < very_close_distance_squared) {
            // Finish recursion
            terrain_collision_t ret = {};
            ret.detected = 1;
            ret.es_collision_point = es_new_sphere_position;
            ret.es_new_velocity = es_new_velocity;
            ret.es_surface_normal = es_slide_plane_normal;

            if (recurse_depth == 0) {
                *first_collision = ret;
            }

            return(ret);
        }
        // There was a collision, must recurse
        else if (recurse_depth < max_recursion_depth/* && slide*/) {
            terrain_collision_t current_collision = {};
            current_collision.detected = 1;
            current_collision.es_collision_point = es_new_sphere_position;
            current_collision.es_new_velocity = es_new_velocity;
            current_collision.es_surface_normal = es_slide_plane_normal;

            if (recurse_depth == 0) {
                *first_collision = current_collision;
            }

            return collide_and_slide(
                es_new_sphere_position * ws_size, ws_size,
                es_new_velocity * ws_size,
                recurse_depth + 1,
                current_collision,
                first_collision);
        }
        else {
            terrain_collision_t ret = {};
            ret.detected = 1;
            ret.es_collision_point = es_new_sphere_position;
            ret.es_new_velocity = es_new_velocity;
            ret.es_surface_normal = es_slide_plane_normal;
            
            return(ret);
        }
    }
    else {
        terrain_collision_t ret = {};

        vector3_t ws_center = es_center * ws_size;
        vector3_t ws_velocity = es_velocity * ws_size;

        ret.es_collision_point = (ws_center + ws_velocity) / ws_size;
        ret.es_new_velocity = ws_velocity / ws_size;
        ret.es_surface_normal = previous_collision.es_surface_normal;
        if (recurse_depth > 0) {
            ret.detected = 1;
            ret.is_currently_in_air = 1;

            *first_collision = ret;
        }
        else {
            ret.detected = 0;
            ret.is_currently_in_air = 1;
        }
        return(ret);
    }
    
    return {};
}
