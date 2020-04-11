#include <string.h>
#include "w_internal.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/tools.hpp>
#include <common/allocators.hpp>
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

    chunk->history.modification_count = 0;
    memset(chunk->history.modification_pool, SPECIAL_VALUE, CHUNK_VOXEL_COUNT);

    chunk->render = NULL;
}

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
    chunk->render->render_data.pbr_info.y = 0.2f;
    chunk->render->render_data.color = vector4_t(0.0f);
}

// Array will be used anytime we need to create mesh from voxels
static vector3_t *temp_mesh_vertices;
static shader_t chunk_shader;

static uint8_t s_chunk_edge_voxel_value(
    int32_t x,
    int32_t y,
    int32_t z,
    bool *doesnt_exist,
    ivector3_t chunk_coord,
    world_t *world) {
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

    chunk_t *chunk_ptr = w_access_chunk(
        ivector3_t(chunk_coord.x + chunk_coord_offset_x,
                   chunk_coord.y + chunk_coord_offset_y,
                   chunk_coord.z + chunk_coord_offset_z),
        world);
    
    *doesnt_exist = (bool)(chunk_ptr == nullptr);
    if (*doesnt_exist) {
        return 0;
    }
    
    return chunk_ptr->voxels[get_voxel_index(final_x, final_y, final_z)];
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

static const ivector3_t NORMALIZED_CUBE_VERTEX_INDICES[8] = {
    ivector3_t(0, 0, 0),
    ivector3_t(1, 0, 0),
    ivector3_t(1, 0, 1),
    ivector3_t(0, 0, 1),
    ivector3_t(0, 1, 0),
    ivector3_t(1, 1, 0),
    ivector3_t(1, 1, 1),
    ivector3_t(0, 1, 1) };

static void s_push_vertex_to_triangle_array(
    uint8_t v0,
    uint8_t v1,
    vector3_t *vertices,
    uint8_t *voxel_values,
    uint8_t surface_level,
    vector3_t *mesh_vertices,
    uint32_t *vertex_count) {
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
    
    mesh_vertices[*(vertex_count)] = vertex;

    ++(*vertex_count);
}

#include "triangle_table.inc"

static void s_update_chunk_mesh_voxel_pair(
    uint8_t *voxel_values,
    uint32_t x,
    uint32_t y,
    uint32_t z,
    uint8_t surface_level,
    vector3_t *mesh_vertices,
    uint32_t *vertex_count) {
    uint8_t bit_combination = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        bool is_over_surface = (voxel_values[i] > surface_level);
        bit_combination |= is_over_surface << i;
    }

    const int8_t *triangle_entry = &TRIANGLE_TABLE[bit_combination][0];

    uint32_t edge = 0;

    int8_t edge_pair[3] = {};

    while(triangle_entry[edge] != -1) {
        int8_t edge_index = triangle_entry[edge];
        edge_pair[edge % 3] = edge_index;

        if (edge % 3 == 2) {
            vector3_t vertices[8] = {};
            for (uint32_t i = 0; i < 8; ++i) {
                vertices[i] = NORMALIZED_CUBE_VERTICES[i] + vector3_t(0.5f) + vector3_t((float)x, (float)y, (float)z);
            }

            for (uint32_t i = 0; i < 3; ++i) {
                switch(edge_pair[i]) {
                case 0: { s_push_vertex_to_triangle_array(0, 1, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 1: { s_push_vertex_to_triangle_array(1, 2, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 2: { s_push_vertex_to_triangle_array(2, 3, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 3: { s_push_vertex_to_triangle_array(3, 0, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 4: { s_push_vertex_to_triangle_array(4, 5, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 5: { s_push_vertex_to_triangle_array(5, 6, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 6: { s_push_vertex_to_triangle_array(6, 7, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 7: { s_push_vertex_to_triangle_array(7, 4, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 8: { s_push_vertex_to_triangle_array(0, 4, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 9: { s_push_vertex_to_triangle_array(1, 5, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 10: { s_push_vertex_to_triangle_array(2, 6, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 11: { s_push_vertex_to_triangle_array(3, 7, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                }
            }
        }

        ++edge;
    }
}

static void s_update_chunk_mesh(
    VkCommandBuffer command_buffer,
    uint8_t surface_level,
    vector3_t *mesh_vertices,
    chunk_t *c,
    world_t *world) {
    uint32_t vertex_count = 0;

    chunk_t *x_superior = w_access_chunk(ivector3_t(c->chunk_coord.x + 1, c->chunk_coord.y, c->chunk_coord.z), world);
    chunk_t *y_superior = w_access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y + 1, c->chunk_coord.z), world);
    chunk_t *z_superior = w_access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y, c->chunk_coord.z + 1), world);
    
    bool doesnt_exist = 0;
    if (x_superior) {
        // x_superior
        for (uint32_t z = 0; z < CHUNK_EDGE_LENGTH; ++z) {
            for (uint32_t y = 0; y < CHUNK_EDGE_LENGTH - 1; ++y) {
                doesnt_exist = 0;
                
                uint32_t x = CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = {
                    c->voxels[get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x]    [y][z + 1],
                    
                    c->voxels[get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z,&doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, world) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist) {
                    s_update_chunk_mesh_voxel_pair(
                        voxel_values,
                        x, y, z,
                        surface_level,
                        mesh_vertices,
                        &vertex_count);
                }
            }
        }
    }

    if (y_superior) {
        // y_superior    
        for (uint32_t z = 0; z < CHUNK_EDGE_LENGTH; ++z) {
            for (uint32_t x = 0; x < CHUNK_EDGE_LENGTH; ++x) {
                doesnt_exist = 0;
                
                uint32_t y = CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = {
                    c->voxels[get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x]    [y][z + 1],
                    
                    s_chunk_edge_voxel_value(x, y + 1, z, &doesnt_exist, c->chunk_coord, world),
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, world) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist) {
                    s_update_chunk_mesh_voxel_pair(
                        voxel_values,
                        x, y, z,
                        surface_level,
                        mesh_vertices,
                        &vertex_count);
                }
            }
        }
    }

    if (z_superior) {
        // z_superior
        for (uint32_t y = 0; y < CHUNK_EDGE_LENGTH - 1; ++y) {
            for (uint32_t x = 0; x < CHUNK_EDGE_LENGTH - 1; ++x) {
                doesnt_exist = 0;
                
                uint32_t z = CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = {
                    c->voxels[get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x]    [y][z + 1],
                    
                    c->voxels[get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, world),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, world) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist) {
                    s_update_chunk_mesh_voxel_pair(
                        voxel_values,
                        x, y, z,
                        surface_level,
                        mesh_vertices,
                        &vertex_count);
                }
            }
        }
    }
    
    for (uint32_t z = 0; z < CHUNK_EDGE_LENGTH - 1; ++z) {
        for (uint32_t y = 0; y < CHUNK_EDGE_LENGTH - 1; ++y) {
            for (uint32_t x = 0; x < CHUNK_EDGE_LENGTH - 1; ++x) {
                uint8_t voxel_values[8] = {
                    c->voxels[get_voxel_index(x, y, z)],
                    c->voxels[get_voxel_index(x + 1, y, z)],
                    c->voxels[get_voxel_index(x + 1, y, z + 1)],
                    c->voxels[get_voxel_index(x, y, z + 1)],
                    
                    c->voxels[get_voxel_index(x, y + 1, z)],
                    c->voxels[get_voxel_index(x + 1, y + 1, z)],
                    c->voxels[get_voxel_index(x + 1, y + 1, z + 1)],
                    c->voxels[get_voxel_index(x, y + 1, z + 1)] };

                s_update_chunk_mesh_voxel_pair(
                    voxel_values,
                    x, y, z,
                    surface_level,
                    mesh_vertices,
                    &vertex_count);
            }
        }
    }

    if (vertex_count) {
        c->flags.active_vertices = 1;

        if (!c->render) {
            w_chunk_render_init(c, w_convert_chunk_to_world(c->chunk_coord));
        }

        static const uint32_t MAX_UPDATE_BUFFER_SIZE = 65536;
        uint32_t update_size = vertex_count * sizeof(vector3_t);

        uint32_t loop_count = update_size / MAX_UPDATE_BUFFER_SIZE;

        typedef char copy_byte_t;
        copy_byte_t *pointer = (copy_byte_t *)mesh_vertices;
        uint32_t to_copy_left = update_size;
        uint32_t copied = 0;
        for (uint32_t i = 0; i < loop_count; ++i) {
            update_gpu_buffer(
                command_buffer,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                copied,
                MAX_UPDATE_BUFFER_SIZE,
                pointer,
                &get_mesh_buffer(BT_VERTEX, &c->render->mesh)->gpu_buffer);

            copied += MAX_UPDATE_BUFFER_SIZE;
            pointer += MAX_UPDATE_BUFFER_SIZE;
            to_copy_left -= MAX_UPDATE_BUFFER_SIZE;
        }

        if (to_copy_left) {
            update_gpu_buffer(
                command_buffer,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                copied,
                to_copy_left,
                pointer,
                &get_mesh_buffer(BT_VERTEX, &c->render->mesh)->gpu_buffer);
        }
        
        c->render->mesh.vertex_count = vertex_count;
    }
    else {
        c->flags.active_vertices = 0;

        if (c->render) {
            w_destroy_chunk_render(c);
            LOG_INFO("Destroyed chunk data\n");
        }
    }
}

static uint8_t surface_level = 70;

void w_chunk_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    world_t *world) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    for (uint32_t i = 0; i < world->chunks.data_count; ++i) {
        chunk_t *c = world->chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame && !world->wait_mesh_update) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                s_update_chunk_mesh(
                    transfer_command_buffer,
                    surface_level,
                    temp_mesh_vertices,
                    c,
                    world);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    &chunk_shader,
                    &c->render->render_data);
            }
        }
    }

#if 0
    if (world->modified_chunk_count > 0) {
        LOG_INFO("Modified some chunks\n");

        for (uint32_t i = 0; i < world->modified_chunk_count; ++i) {
            chunk_t *c = world->modified_chunks[i];
            (void)c;
        }
    }
#endif
}

void w_destroy_chunk_render(
    chunk_t *chunk) {
    if (chunk->render) {
        destroy_sensitive_gpu_buffer(get_mesh_buffer(BT_VERTEX, &chunk->render->mesh)->gpu_buffer);
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
    static std::hash<glm::ivec3> hasher;

    return (uint32_t)hasher(coord);
}

void w_chunks_data_init() {
    temp_mesh_vertices = FL_MALLOC(vector3_t, MAX_VERTICES_PER_CHUNK);

    mesh_t chunk_mesh_prototype = {};
    push_buffer_to_mesh(
        BT_VERTEX,
        &chunk_mesh_prototype);

    shader_binding_info_t binding_info = create_mesh_binding_info(
        &chunk_mesh_prototype);
    
    const char *shader_paths[] = {
        "shaders/SPV/chunk_mesh.vert.spv",
        "shaders/SPV/chunk_mesh.geom.spv",
        "shaders/SPV/chunk_mesh.frag.spv" };
    
    chunk_shader = create_mesh_shader_color(
        &binding_info,
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_CULL_MODE_FRONT_BIT);
}

void w_clear_chunk_world(
    world_t *world) {
    if (world->chunks.data_count) {
        for (uint32_t i = 0; i < world->chunks.data_count; ++i) {
            w_destroy_chunk(world->chunks[i]);
            world->chunks[i] = NULL;
        }

        world->chunks.clear();
        world->chunk_indices.clear();

        LOG_INFO("Destroyed chunk world\n");
    }
}

void w_destroy_chunk_data() {
    FL_FREE(temp_mesh_vertices);
}

void w_chunk_world_init(
    world_t *world,
    uint32_t loaded_radius) {
    world->chunk_indices.init();

    world->loaded_radius = loaded_radius;

    world->chunks.init(MAX_LOADED_CHUNKS);

    world->max_modified_chunks = MAX_LOADED_CHUNKS / 2;
    world->modified_chunks = FL_MALLOC(chunk_t *, world->max_modified_chunks);
    world->modified_chunk_count = 0;

    world->track_history = 1;

    world->wait_mesh_update = 0;

    w_add_sphere_m(vector3_t(70.0f, 90.0f, -90.0f), 25.0f, world);
    w_add_sphere_m(vector3_t(80.0f), 17.0f, world);

    w_add_sphere_m(vector3_t(0.0f), 40.0f, world);
    w_add_sphere_m(vector3_t(-70.0f), 30.0f, world);

    w_add_sphere_m(vector3_t(70.0f, -90.0f, 45.0f), 25.0f, world);
}

void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius,
    world_t *world) {
    ivector3_t vs_center = w_convert_world_to_voxel(ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = w_convert_voxel_to_chunk(vs_center);

    chunk_t *current_chunk = w_get_chunk(
        current_chunk_coord,
        world);

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
                    ivector3_t c = w_convert_voxel_to_chunk(vs_position);

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
                        uint8_t new_value = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);
                        if (*v < new_value) {
                            *v = new_value;
                        }
                    }
                    else {
                        ivector3_t c = w_convert_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = w_get_chunk(c, world);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = 1.0f - (distance_squared / radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        uint8_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);
                        if (*v < new_value) {
                            *v = new_value;
                        }
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

chunk_t *w_access_chunk(
    const ivector3_t &coord,
    world_t *world) {
    uint32_t hash = w_hash_chunk_coord(coord);
    uint32_t *index = world->chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return world->chunks[*index];
    }
    else {
        return NULL;
    }
}

chunk_t *w_get_chunk(
    const ivector3_t &coord,
    world_t *world) {
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

static void s_terraform_with_history(
    terraform_type_t type,
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    float radius,
    float speed,
    float delta_time,
    world_t *world) {
    vector3_t vs_position = ws_ray_start;
    vector3_t vs_dir = ws_ray_direction;

    static const float PRECISION = 1.0f / 10.0f;
    
    vector3_t vs_step = vs_dir * max_reach * PRECISION;

    float max_reach_squared = max_reach * max_reach;

    // If the delta time is too small, it is possible that the chunks won't get modified at all
    // Therefore, if the delta time is very small, there needs to be a sort of accumulation of delta times

    static float dt = 0.0f;

    dt += delta_time;
    
    float max_change = 1.0f * dt * speed;
    int32_t max_change_i = (int32_t)max_change;

    if (max_change_i < 2) {
        // Don't do anything
    }
    else {
        for (; glm::dot(vs_position - ws_ray_start, vs_position - ws_ray_start) < max_reach_squared; vs_position += vs_step) {
            ivector3_t voxel = w_convert_world_to_voxel(vs_position);
            ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
            chunk_t *chunk = w_access_chunk(chunk_coord, world);

            if (chunk) {
                ivector3_t local_voxel_coord = w_convert_voxel_to_local_chunk(voxel);
                if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > surface_level) {
                    if (!chunk->flags.made_modification) {
                        // Push this chunk onto list of modified chunks
                        world->modified_chunks[world->modified_chunk_count++] = chunk;
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
                                        ivector3_t chunk_coord = w_convert_voxel_to_chunk(current_voxel);
                                        chunk_t *new_chunk = w_get_chunk(chunk_coord, world);

                                        chunk = new_chunk;

                                        if (!chunk->flags.made_modification) {
                                            // Push this chunk onto list of modified chunks
                                            world->modified_chunks[world->modified_chunk_count++] = chunk;
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
                                    uint8_t *voxel = &chunk->voxels[voxel_index];
                                    uint8_t voxel_value = *voxel;
                                    float proportion = 1.0f - (distance_squared / radius_squared);

                                    int32_t current_voxel_value = (int32_t)*voxel;

                                    int32_t new_value = (int32_t)(proportion * coeff * dt * speed) + current_voxel_value;

                                    uint8_t *vh = &chunk->history.modification_pool[voxel_index];
                                    
                                    if (new_value > (int32_t)MAX_VOXEL_VALUE_I) {
                                        voxel_value = (int32_t)MAX_VOXEL_VALUE_I;
                                    }
                                    else if (new_value < 0) {
                                        voxel_value = 0;
                                    }
                                    else {
                                        voxel_value = (uint8_t)new_value;
                                    }

                                    // Didn't add to the history yet
                                    if (*vh == SPECIAL_VALUE && voxel_value != *voxel) {
                                        *vh = *voxel;
                                        chunk->history.modification_stack[chunk->history.modification_count++] = voxel_index;
                                    }
                                    
                                    *voxel = voxel_value;
                                }
                            }
                        }
                    }

                    break;
                }
            }
        }

        dt = 0.0f;
    }
}

static void s_terraform_without_history(
    terraform_type_t type,
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    float radius,
    float speed,
    float delta_time,
    world_t *world) {
    vector3_t vs_position = ws_ray_start;
    vector3_t vs_dir = ws_ray_direction;

    static const float PRECISION = 1.0f / 10.0f;
    
    vector3_t vs_step = vs_dir * max_reach * PRECISION;

    float max_reach_squared = max_reach * max_reach;

    // If the delta time is too small, it is possible that the chunks won't get modified at all
    // Therefore, if the delta time is very small, there needs to be a sort of accumulation of delta times

    static float dt = 0.0f;

    dt += delta_time;
    
    float max_change = 1.0f * dt * speed;
    int32_t max_change_i = (int32_t)max_change;

    if (max_change_i < 2) {
        // Don't do anything
    }
    else {
        for (; glm::dot(vs_position - ws_ray_start, vs_position - ws_ray_start) < max_reach_squared; vs_position += vs_step) {
            ivector3_t voxel = w_convert_world_to_voxel(vs_position);
            ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
            chunk_t *chunk = w_access_chunk(chunk_coord, world);

            if (chunk) {
                ivector3_t local_voxel_coord = w_convert_voxel_to_local_chunk(voxel);
                if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > surface_level) {
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
                                        ivector3_t chunk_coord = w_convert_voxel_to_chunk(current_voxel);
                                        chunk_t *new_chunk = w_get_chunk(chunk_coord, world);

                                        chunk = new_chunk;
                                        chunk->flags.made_modification = 1;
                                        chunk->flags.has_to_update_vertices = 1;

                                        current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                    }

                                    uint8_t *voxel = &chunk->voxels[get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z)];
                                    float proportion = 1.0f - (distance_squared / radius_squared);

                                    int32_t current_voxel_value = (int32_t)*voxel;

                                    int32_t new_value = (int32_t)(proportion * coeff * dt * speed) + current_voxel_value;

                                    if (new_value > (int32_t)MAX_VOXEL_VALUE_I) {
                                        *voxel = (int32_t)MAX_VOXEL_VALUE_I;
                                    }
                                    else if (new_value < 0) {
                                        *voxel = 0;
                                    }
                                    else {
                                        *voxel = (uint8_t)new_value;
                                    }
                                }
                            }
                        }
                    }

                    break;
                }
            }
        }

        dt = 0.0f;
    }
}

void w_terraform(
    terraform_type_t type,
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach,
    float radius,
    float speed,
    float delta_time,
    world_t *world) {
    if (world->track_history) {
        s_terraform_with_history(
            type,
            ws_ray_start,
            ws_ray_direction,
            max_reach,
            radius,
            speed,
            delta_time,
            world);
    }
    else {
        s_terraform_without_history(
            type,
            ws_ray_start,
            ws_ray_direction,
            max_reach,
            radius,
            speed,
            delta_time,
            world);
    }
}

void w_toggle_mesh_update_wait(
    bool value,
    world_t *world) {
    world->wait_mesh_update = value;
}

void activate_chunk_history(
    chunk_t *chunk) {
    //chunk->history = FL_MALLOC(chunk_history_t, 1);
    chunk->history.modification_count = 0;
    memset(chunk->history.modification_pool, SPECIAL_VALUE, CHUNK_VOXEL_COUNT);
}
