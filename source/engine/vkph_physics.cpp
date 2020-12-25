#include "engine/vkph_terraform.hpp"
#include "vkph_state.hpp"
#include "vkph_chunk.hpp"
#include "vkph_player.hpp"
#include "vkph_physics.hpp"
#include "vkph_physics_triangle_table.hpp"

#include <glm/gtx/projection.hpp>

namespace vkph {

bool compute_acceleration_vector(
    struct player_t *player,
    struct player_action_t *actions,
    movement_resolution_flags_t flags,
    force_values_t *force_values,
    vector3_t *dst) {
    movement_axes_t axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);

    bool made_movement = 0;
    vector3_t final_acceleration = vector3_t(0.0f);
    
    if (actions->move_forward) {
        final_acceleration += axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_left) {
        final_acceleration -= axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_back) {
        final_acceleration -= axes.forward * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->move_right) {
        final_acceleration += axes.right * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->jump) {
        final_acceleration += axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }
    if (actions->crouch) {
        final_acceleration -= axes.up * actions->dt * player->default_speed;
        made_movement = 1;
    }

    *dst = final_acceleration;

    return made_movement;
}

movement_axes_t compute_movement_axes(
    const vector3_t &view_direction,
    const vector3_t &up) {
    vector3_t right = glm::normalize(glm::cross(view_direction, up));
    vector3_t forward = glm::normalize(glm::cross(up, right));
    movement_axes_t axes = {right, up, forward};
    return axes;
}

void compute_velocity_vector(
    player_t *player,
    player_action_t *actions,
    force_values_t *force_values,
    movement_resolution_flags_t flags,
    const vector3_t &acceleration) {
    
    player->ws_velocity += acceleration * actions->dt * force_values->movement_acceleration;

    // Do some check between standing and ball mode
    if (flags & MRF_GRAVITY_CHECK_INCLINATION) {
        if (glm::dot(player->ws_up_vector, player->ws_surface_normal) < 0.7f || !player->flags.is_on_ground) {
            player->ws_velocity += -player->ws_up_vector * force_values->gravity * actions->dt;
        }
        else {
        }
    }
    else {
        player->ws_velocity += -player->ws_up_vector * force_values->gravity * actions->dt;
    }

    if (player->flags.is_on_ground) {
        player->ws_velocity += -player->ws_velocity * actions->dt * force_values->friction;
    }
    else {
    }
}

void resolve_player_movement(
    player_t *player,
    player_action_t *actions,
    force_values_t *force_values,
    int32_t flags) {
    vector3_t acceleration = vector3_t(0.0f);
    bool made_movement = compute_acceleration_vector(player, actions, (movement_resolution_flags_t)flags, force_values, &acceleration);
        
    if (made_movement && glm::dot(acceleration, acceleration) != 0.0f) {
        acceleration = glm::normalize(acceleration);
        player->flags.moving = 1;
    }
    else {
        player->flags.moving = 0;
    }

    if (!made_movement && player->flags.is_on_ground && flags & MRF_ABRUPT_STOP) {
        //player->ws_velocity = vector3_t(0.0f);
    }

    compute_velocity_vector(player, actions, force_values, (movement_resolution_flags_t)flags, acceleration);

    terrain_collision_t collision = {};
    collision.ws_size = vector3_t(PLAYER_SCALE);
    collision.ws_position = player->ws_position;
    collision.ws_velocity = player->ws_velocity * actions->dt;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    vector3_t ws_new_position = collide_and_slide(&collision) * PLAYER_SCALE;

    if (player->flags.is_on_ground) {
        float len = glm::length(ws_new_position - player->ws_position);
        if (len < 0.00001f) {
            player->frame_displacement = 0.0f;
        }
        else {
            player->frame_displacement = len;
        }
    }

    player->ws_position = ws_new_position;

    if (collision.detected) {
        vector3_t normal = glm::normalize(collision.es_surface_normal * PLAYER_SCALE);
        player->ws_surface_normal = normal;

        if (!player->flags.is_on_ground) {
            if (glm::abs(glm::dot(player->ws_velocity, player->ws_velocity)) == 0.0f) {
                float previous_velocity_length = glm::length(player->ws_velocity);
                movement_axes_t new_axes = compute_movement_axes(player->ws_view_direction, player->ws_up_vector);
                player->ws_velocity = glm::normalize(glm::proj(player->ws_velocity, new_axes.forward)) * previous_velocity_length * 0.2f;
            }
        }

        if (flags & MRF_ADOPT_GRAVITY) {
            player->ws_up_vector = normal;
            player->next_camera_up = normal;
        }

        float vdotv = glm::dot(player->ws_velocity, player->ws_velocity);
        if (glm::abs(vdotv) == 0.0f) {
            player->ws_velocity = vector3_t(0.0f);
        }
        else {
            // Apply normal force
            player->ws_velocity += player->ws_up_vector * force_values->gravity * actions->dt;
        }
        player->flags.is_on_ground = 1;
    }
    else {
        player->flags.is_on_ground = 0;
    }
}


enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };

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
    ivector3_t chunk_coord,
    const state_t *state) {
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

    const chunk_t *chunk_ptr = state->access_chunk(
        ivector3_t(chunk_coord.x + chunk_coord_offset_x,
                   chunk_coord.y + chunk_coord_offset_y,
                   chunk_coord.z + chunk_coord_offset_z));
    
    *doesnt_exist = (bool)(chunk_ptr == nullptr);
    if (*doesnt_exist) {
        return 0;
    }
    
    return chunk_ptr->voxels[get_voxel_index(final_x, final_y, final_z)].value;
}

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
    const vector3_t &ws_size,
    const state_t *state) {
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
                chunk_t *chunk = state->access_chunk(chunk_coord);

                if (chunk) {
                    bool doesnt_exist = 0;
                
                    uint8_t voxel_values[8] = {};
                    
                    ivector3_t cs_coord = space_voxel_to_local_chunk(voxel_coord);
                    
                    if (is_between_chunks) {
                        voxel_values[0] = chunk->voxels[get_voxel_index(cs_coord.x, cs_coord.y, cs_coord.z)].value;
                        voxel_values[1] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z, &doesnt_exist, chunk_coord, state);
                        voxel_values[2] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord, state);
                        voxel_values[3] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y, cs_coord.z + 1, &doesnt_exist, chunk_coord, state);
                        
                        voxel_values[4] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord, state);
                        voxel_values[5] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z, &doesnt_exist, chunk_coord, state);
                        voxel_values[6] = s_chunk_edge_voxel_value(cs_coord.x + 1, cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord, state);
                        voxel_values[7] = s_chunk_edge_voxel_value(cs_coord.x,     cs_coord.y + 1, cs_coord.z + 1, &doesnt_exist, chunk_coord, state);
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

static bool s_collided_with_triangle(terrain_collision_t *collision, collision_triangle_t *es_triangle) {
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

vector3_t collide_and_slide(terrain_collision_t *collision, const state_t *state) {
    // Get intersecting triangles
    uint32_t triangle_count = 0;
    collision_triangle_t *triangles = s_get_collision_triangles(&triangle_count, collision->ws_position, collision->ws_size, state);

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

        return collide_and_slide(collision, state);
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

        return collide_and_slide(collision, state);
    }
}

void check_ray_terrain_collision(terrain_collision_t *collision, const state_t *state) {
    uint32_t triangle_count = 0;
    collision_triangle_t *triangles = s_get_collision_triangles(&triangle_count, collision->ws_position, collision->ws_size, state);

    for (uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        collision_triangle_t *triangle = &triangles[triangle_index];

        for (uint32_t i = 0; i < 3; ++i) {
            triangle->vertices[i] /= collision->ws_size;
        }

        // Check collision with this triangle (now is ellipsoid space)
        s_collided_with_triangle(collision, triangle);
    }
}

terraform_info_t cast_terrain_ray(
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    voxel_color_t color,
    const state_t *state) {
    vector3_t vs_position = ws_ray_start;
    vector3_t vs_dir = ws_ray_direction;

    static const float PRECISION = 1.0f / 15.0f;
    
    vector3_t vs_step = vs_dir * max_reach * PRECISION;

    float max_reach_squared = max_reach * max_reach;

    terraform_info_t package = {};
    package.ray_hit_terrain = 0;
    package.color = color;

    for (; glm::dot(vs_position - ws_ray_start, vs_position - ws_ray_start) < max_reach_squared; vs_position += vs_step) {
        ivector3_t voxel = space_world_to_voxel(vs_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        const chunk_t *chunk = state->access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            const voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];

            terrain_collision_t collision = {};
            collision.ws_size = vector3_t(0.1f);
            collision.ws_position = vs_position;
            collision.ws_velocity = vs_dir;
            collision.es_position = collision.ws_position / collision.ws_size;
            collision.es_velocity = collision.ws_velocity / collision.ws_size;

            check_ray_terrain_collision(&collision, state);

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

}
