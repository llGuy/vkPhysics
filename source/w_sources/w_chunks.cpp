#include "net.hpp"
#include <string.h>
#include "w_internal.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/tools.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

static terraform_package_t local_current_terraform_package;

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
