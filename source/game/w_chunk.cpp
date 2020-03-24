#include "w_internal.hpp"
#include <common/tools.hpp>
#include <renderer/renderer.hpp>

uint32_t get_voxel_index(
    uint32_t x,
    uint32_t y,
    uint32_t z) {
    return z * (CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH) + y * CHUNK_EDGE_LENGTH + x;
}

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

// Array will be used anytime we need to create mesh from voxels
static vector3_t *temp_mesh_vertices;
static shader_t chunk_shader;

void w_chunk_gpu_sync(
    VkCommandBuffer command_buffer,
    chunk_world_t *world) {
    for (uint32_t i = 0; i < world->chunks.data_count; ++i) {
        chunk_t *c = world->chunks[i];

        // Create render struct if not created yet
        if (!c->render) {
            w_chunk_render_init(c, w_convert_chunk_to_world(c->chunk_coord), vector3_t(1.0f));
        }

        if (c->flags.made_modification) {
            // Update chunk mesh and put on GPU + send to command buffer
            // TODO:
        }
    }
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

void w_chunk_data_init() {
    temp_mesh_vertices = FL_MALLOC(vector3_t, MAX_VERTICES_PER_CHUNK);

    mesh_t chunk_mesh_prototype = {};
    push_buffer_to_mesh(
        BT_VERTEX,
        &chunk_mesh_prototype);

    shader_binding_info_t binding_info = create_mesh_binding_info(
        &chunk_mesh_prototype);
    
    const char *shader_paths[] = {
        "../shaders/SPV/chunk_mesh.vert.spv",
        "../shaders/SPV/chunk_mesh.geom.spv",
        "../shaders/SPV/chunk_mesh.frag.spv" };
    
    chunk_shader = create_mesh_shader_color(
        &binding_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

void w_chunk_world_init(
    chunk_world_t *world,
    uint32_t loaded_radius) {
    world->loaded_radius = loaded_radius;

    world->chunks.init(MAX_LOADED_CHUNKS);
}

void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius,
    chunk_world_t *world) {
    ivector3_t vs_center = w_convert_world_to_voxel(ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = w_convert_voxel_to_chunk(vs_center);

    chunk_t *current_chunk = w_get_chunk(
        current_chunk_coord,
        world);

    current_chunk->flags.made_modification = 1;

    int32_t diameter = (int32_t)ws_radius * 2 + 1;

    int32_t start_z = vs_center.z - (int32_t)ws_radius;
    int32_t start_y = vs_center.z - (int32_t)ws_radius;
    int32_t start_x = vs_center.z - (int32_t)ws_radius;

    float radius_squared = ws_radius * ws_radius;

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                ivector3_t vs_position = ivector3_t(x, y, z);
                vector3_t vs_float = vector3_t((float)x, (float)y, (float)z);
                vector3_t vs_diff_float = vs_float - vs_float_center;

                float distance_squared = glm::dot(vs_diff_float, vs_diff_float);

                if (distance_squared <= radius_squared) {
                    ivector3_t c = w_convert_voxel_to_chunk(vs_position);

                    if (c.x == current_chunk_coord.x && c.y == current_chunk_coord.y && c.z == current_chunk_coord.z) {
                        // Is within current chunk boundaries
                        float proportion = 1.0f - (distance_squared / radius_squared);

                        ivector3_t voxel_coord = w_convert_voxel_to_local_chunk(vs_position);

                        current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) / (float)MAX_VOXEL_VALUE_I);
                    }
                    else {
                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = w_get_chunk(vs_position, world);

                        current_chunk->flags.made_modification = 1;

                        float proportion = 1.0f - (distance_squared / radius_squared);

                        ivector3_t voxel_coord = w_convert_voxel_to_local_chunk(vs_position);

                        current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) / (float)MAX_VOXEL_VALUE_I);
                    }
                }
            }
        }
    }
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

vector3_t w_convert_chunk_to_world(
    const ivector3_t &chunk_coord) {
    vector3_t ws_coord = (vector3_t)(chunk_coord);
    return ws_coord * (float)CHUNK_EDGE_LENGTH;
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
    uint32_t hash = w_hash_chunk_coord(coord);
    uint32_t *index = world->chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return world->chunks[*index];
    }
    else {
        uint32_t i = world->chunks.add();
        chunk_t *&chunk = world->chunks[i];
        chunk = FL_MALLOC(chunk_t, 1);
        w_chunk_init(chunk, i, coord);

        world->chunk_indices.insert(hash, i);

        return chunk;
    }
}
