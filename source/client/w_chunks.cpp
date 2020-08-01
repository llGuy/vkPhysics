#include "net.hpp"
#include <string.h>
#include "w_internal.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/tools.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

static terraform_package_t local_current_terraform_package;

void w_chunk_render_init(
    chunk_t *chunk,
    const vector3_t &ws_position) {
    chunk->render = FL_MALLOC(chunk_render_t, 1);

    memset(chunk->render, 0, sizeof(chunk_render_t));

    uint32_t buffer_size = sizeof(vector3_t) * MAX_VERTICES_PER_CHUNK;

    push_buffer_to_mesh(BT_VERTEX, &chunk->render->mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, &chunk->render->mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        buffer_size,
        NULL,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    chunk->render->mesh.vertex_offset = 0;
    chunk->render->mesh.vertex_count = 0;
    chunk->render->mesh.first_index = 0;
    chunk->render->mesh.index_offset = 0;
    chunk->render->mesh.index_count = 0;
    chunk->render->mesh.index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(&chunk->render->mesh);

    chunk->render->render_data.model = glm::translate(ws_position);
    chunk->render->render_data.pbr_info.x = 0.1f;
    chunk->render->render_data.pbr_info.y = 0.1f;
    chunk->render->render_data.color = vector4_t(0.0f);
}



static chunks_to_interpolate_t chunks_to_interpolate;

void w_tick_chunks(
    float dt) {
    chunks_to_interpolate.elapsed += dt;
    float progression = chunks_to_interpolate.elapsed / server_snapshot_interval();

    if (progression >= 1.0f) {
        progression = 1.0f;
    }
    
    for (uint32_t cm_index = 0; cm_index < chunks_to_interpolate.modification_count; ++cm_index) {
        chunk_modifications_t *cm_ptr = &chunks_to_interpolate.modifications[cm_index];
        chunk_t *c_ptr = get_chunk(ivector3_t(cm_ptr->x, cm_ptr->y, cm_ptr->z));

        c_ptr->flags.has_to_update_vertices = 1;
        for (uint32_t vm_index = 0; vm_index < cm_ptr->modified_voxels_count; ++vm_index) {
            voxel_modification_t *vm_ptr = &cm_ptr->modifications[vm_index];
            uint8_t *current_value = &c_ptr->voxels[vm_ptr->index];
            float fcurrent_value = (float)(*current_value);
            float initial_value = (float)(vm_ptr->initial_value);
            float final_value = (float)(vm_ptr->final_value);

            fcurrent_value = interpolate(initial_value, final_value, progression);
            if (fcurrent_value < 0.0f) {
                fcurrent_value = 0.0f;
            }
            else if (fcurrent_value > 254.0f) {
                fcurrent_value = 254.0f;
            }
            *current_value = (uint8_t)fcurrent_value;
        }
    }
}

void w_destroy_chunk_render(
    chunk_t *chunk) {
    if (chunk->render) {
        destroy_sensitive_gpu_buffer(get_mesh_buffer(BT_VERTEX, &chunk->render->mesh)->gpu_buffer);
        FL_FREE(chunk->render);
        chunk->render = NULL;
    }
}

void w_clear_chunks_and_render_rsc() {
    uint32_t chunk_count;
    chunk_t **chunks = get_active_chunks(&chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        // Destroy the chunk's rendering resources
        w_destroy_chunk_render(chunks[i]);

        destroy_chunk(chunks[i]);

        chunks[i] = NULL;
    }

    clear_chunks();
}

chunks_to_interpolate_t *get_chunks_to_interpolate() {
    return &chunks_to_interpolate;
}

void w_chunk_world_init(
    uint32_t loaded_radius) {
    chunk_memory_init();

    chunks_to_interpolate.max_modified = 40;
    chunks_to_interpolate.modification_count = 0;
    chunks_to_interpolate.modifications = FL_MALLOC(chunk_modifications_t, chunks_to_interpolate.max_modified);
    memset(chunks_to_interpolate.modifications, 0, sizeof(chunk_modifications_t) * chunks_to_interpolate.max_modified);

// #if 1
//     w_add_sphere_m(vector3_t(20.0f), 40.0f);
//     w_add_sphere_m(vector3_t(-40.0f, 40.0f, -40.0f), 20.0f);
//     w_add_sphere_m(vector3_t(-70.0f, 90.0f, 45.0f), 25.0f);
// #endif

#if 0
    for (int32_t z = -32; z < 32; ++z) {
        for (int32_t x = -32; x < 32; ++x) {
            ivector3_t voxel_coord = ivector3_t((float)x, -2.0f, (float)z);
            ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_coord);
            chunk_t *chunk = w_get_chunk(chunk_coord);
            chunk->flags.has_to_update_vertices = 1;
            ivector3_t local_coord = w_convert_voxel_to_local_chunk(voxel_coord);
            uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
            chunk->voxels[index] = 80;
        }
    }

    for (uint32_t z = 31; z < 64; ++z) {
        for (uint32_t x = 0; x < 32; ++x) {
            ivector3_t voxel_coord = ivector3_t((float)x, 5.0f, (float)z);
            ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_coord);
            chunk_t *chunk = w_get_chunk(chunk_coord);
            chunk->flags.has_to_update_vertices = 1;
            ivector3_t local_coord = w_convert_voxel_to_local_chunk(voxel_coord);
            uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
            chunk->voxels[index] = 80;
        }
    }
#endif
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

void cast_ray_sensors(
    sensors_t *sensors,
    const vector3_t &ws_position,
    const vector3_t &ws_view_direction,
    const vector3_t &ws_up_vector) {
    const vector3_t ws_right_vector = glm::normalize(glm::cross(ws_view_direction, ws_up_vector));

    // Need to cast 26 rays
    vector3_t rays[AI_SENSOR_COUNT] = {
        ws_view_direction, 
        glm::normalize(ws_view_direction - ws_up_vector - ws_right_vector),
        glm::normalize(ws_view_direction - ws_right_vector),
        glm::normalize(ws_view_direction + ws_up_vector - ws_right_vector),
        glm::normalize(ws_view_direction + ws_up_vector),
        glm::normalize(ws_view_direction + ws_up_vector + ws_right_vector),
        glm::normalize(ws_view_direction + ws_right_vector),
        glm::normalize(ws_view_direction - ws_up_vector + ws_right_vector),
        glm::normalize(ws_view_direction - ws_up_vector),
        
        -ws_view_direction, 
        glm::normalize(-ws_view_direction - ws_up_vector - ws_right_vector),
        glm::normalize(-ws_view_direction - ws_right_vector),
        glm::normalize(-ws_view_direction + ws_up_vector - ws_right_vector),
        glm::normalize(-ws_view_direction + ws_up_vector),
        glm::normalize(-ws_view_direction + ws_up_vector + ws_right_vector),
        glm::normalize(-ws_view_direction + ws_right_vector),
        glm::normalize(-ws_view_direction - ws_up_vector + ws_right_vector),
        glm::normalize(-ws_view_direction - ws_up_vector),

        glm::normalize(-ws_up_vector),
        glm::normalize(-ws_up_vector - ws_right_vector),
        glm::normalize(-ws_right_vector),
        glm::normalize(ws_up_vector - ws_right_vector),
        glm::normalize(ws_up_vector),
        glm::normalize(ws_up_vector + ws_right_vector),
        glm::normalize(ws_right_vector),
        glm::normalize(-ws_up_vector + ws_right_vector),
    };

    static const float PRECISION = 1.0f / 10.0f;
    float max_reach = 10.0f;
    float max_reach_squared = max_reach * max_reach;

    for (uint32_t i = 0; i < AI_SENSOR_COUNT; ++i) {
        vector3_t step = rays[i] * max_reach * PRECISION;

        vector3_t current_ray_position = ws_position;

        float length_step = max_reach * PRECISION;
        float length = 0.0f;

        bool hit = 0;

        for (; glm::dot(current_ray_position - ws_position, current_ray_position - ws_position) < max_reach_squared; current_ray_position += step) {
            ivector3_t voxel = space_world_to_voxel(current_ray_position);
            ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
            chunk_t *chunk = access_chunk(chunk_coord);

            length += length_step;

            if (chunk) {
                ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
                if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > CHUNK_SURFACE_LEVEL) {

                    break;
                }
            }
        }

        sensors->s[i] = length;
    }
}

terraform_package_t *w_get_local_current_terraform_package() {
    return &local_current_terraform_package;
}
