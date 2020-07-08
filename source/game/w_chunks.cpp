#include "net.hpp"
#include <string.h>
#include "w_internal.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/tools.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

struct chunks_t {
    uint32_t loaded_radius;
    // List of chunks
    // Works like a stack
    stack_container_t<chunk_t *> chunks;
    uint32_t render_count;
    chunk_t **chunks_to_render;
    hash_table_t<uint32_t, 300, 30, 10> chunk_indices;

    uint32_t max_modified_chunks;
    uint32_t modified_chunk_count;
    chunk_t **modified_chunks;

    // Used for terrain pointer
    terraform_package_t local_current_terraform_package;

    struct {
        uint8_t wait_mesh_update: 1;
        uint8_t track_history: 1;
    };
};

static chunks_t chunks;

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

    chunk->flags.made_modification = 0;
    chunk->flags.has_to_update_vertices = 0;
    chunk->flags.active_vertices = 0;
    chunk->flags.modified_marker = 0;
    chunk->flags.index_of_modification_struct = 0;

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
    chunk->render->render_data.pbr_info.y = 0.1f;
    chunk->render->render_data.color = vector4_t(0.0f);
}

// Array will be used anytime we need to create mesh from voxels
static vector3_t *temp_mesh_vertices;
static shader_t chunk_shader;

static struct chunk_color_data_t {
    vector4_t pointer_position;
    vector4_t pointer_color;
    float pointer_radius;
} chunk_color_data;

static gpu_buffer_t chunk_color_data_buffer;
static VkDescriptorSet chunk_color_data_buffer_set;

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

uint32_t w_create_chunk_vertices(
    uint8_t surface_level,
    vector3_t *mesh_vertices,
    chunk_t *c) {
    uint32_t vertex_count = 0;

    chunk_t *x_superior = access_chunk(ivector3_t(c->chunk_coord.x + 1, c->chunk_coord.y, c->chunk_coord.z));
    chunk_t *y_superior = access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y + 1, c->chunk_coord.z));
    chunk_t *z_superior = access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y, c->chunk_coord.z + 1));
    
    bool doesnt_exist = 0;
    if (x_superior) {
        // x_superior
        for (uint32_t z = 0; z < CHUNK_EDGE_LENGTH; ++z) {
            for (uint32_t y = 0; y < CHUNK_EDGE_LENGTH - 1; ++y) {
                doesnt_exist = 0;
                
                uint32_t x = CHUNK_EDGE_LENGTH - 1;

                uint8_t voxel_values[8] = {
                    c->voxels[get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x]    [y][z + 1],
                    
                    c->voxels[get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z,&doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord) };//voxels[x]    [y + 1][z + 1] };

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
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x]    [y][z + 1],
                    
                    s_chunk_edge_voxel_value(x, y + 1, z, &doesnt_exist, c->chunk_coord),
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord) };//voxels[x]    [y + 1][z + 1] };

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
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x]    [y][z + 1],
                    
                    c->voxels[get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord) };//voxels[x]    [y + 1][z + 1] };

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

    return vertex_count;
}

static void s_update_chunk_mesh(
    VkCommandBuffer command_buffer,
    uint8_t surface_level,
    vector3_t *mesh_vertices,
    chunk_t *c) {
    uint32_t vertex_count = w_create_chunk_vertices(
        surface_level,
        mesh_vertices,
        c);

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

static uint8_t surface_level = 70;

uint8_t w_get_surface_level() {
    return surface_level;
}

void w_chunk_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    if (chunks.local_current_terraform_package.ray_hit_terrain) {
        chunk_color_data.pointer_radius = 3.0f;
    }
    else {
        chunk_color_data.pointer_radius = 0.0f;
    }

    chunk_color_data.pointer_position = vector4_t(chunks.local_current_terraform_package.position, 1.0f);
    chunk_color_data.pointer_color = vector4_t(0.0f, 1.0f, 1.0f, 1.0f);

    update_gpu_buffer(
        transfer_command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(chunk_color_data_t),
        &chunk_color_data,
        &chunk_color_data_buffer);

    for (uint32_t i = 0; i < chunks.chunks.data_count; ++i) {
        chunk_t *c = chunks.chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame && !chunks.wait_mesh_update) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                s_update_chunk_mesh(
                    transfer_command_buffer,
                    surface_level,
                    temp_mesh_vertices,
                    c);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    &chunk_shader,
                    &c->render->render_data,
                    chunk_color_data_buffer_set);
            }
        }
    }

#if 0
    if (chunks.modified_chunk_count > 0) {
        LOG_INFO("Modified some chunks\n");

        for (uint32_t i = 0; i < chunks.modified_chunk_count; ++i) {
            chunk_t *c = chunks.modified_chunks[i];
            (void)c;
        }
    }
#endif
}

void w_render_startup_world(
    VkCommandBuffer render_command_buffer) {
    startup_screen_t *startup_data = w_get_startup_screen_data();

    submit_mesh(
        render_command_buffer,
        &startup_data->world_mesh,
        &chunk_shader,
        &startup_data->world_render_data,
        chunk_color_data_buffer_set);
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
        VK_CULL_MODE_FRONT_BIT,
        MT_STATIC | MT_PASS_EXTRA_UNIFORM_BUFFER);

    chunk_color_data.pointer_position = vector4_t(0.0f);
    chunk_color_data.pointer_color = vector4_t(0.0f);
    chunk_color_data.pointer_radius = 0.0f;

    chunk_color_data_buffer = create_gpu_buffer(
        sizeof(chunk_color_data_t),
        &chunk_color_data,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    chunk_color_data_buffer_set = create_buffer_descriptor_set(
        chunk_color_data_buffer.buffer,
        sizeof(chunk_color_data_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void w_clear_chunk_world() {
    if (chunks.chunks.data_count) {
        for (uint32_t i = 0; i < chunks.chunks.data_count; ++i) {
            w_destroy_chunk(chunks.chunks[i]);
            chunks.chunks[i] = NULL;
        }

        chunks.chunks.clear();
        chunks.chunk_indices.clear();

        LOG_INFO("Destroyed chunk world\n");
    }
}

void w_destroy_chunk_data() {
    FL_FREE(temp_mesh_vertices);
}

chunks_to_interpolate_t *get_chunks_to_interpolate() {
    return &chunks_to_interpolate;
}

void w_chunk_world_init(
    uint32_t loaded_radius) {
    chunks.chunk_indices.init();

    chunks.loaded_radius = loaded_radius;

    chunks.chunks.init(MAX_LOADED_CHUNKS);

    chunks.max_modified_chunks = MAX_LOADED_CHUNKS / 2;
    chunks.modified_chunks = FL_MALLOC(chunk_t *, chunks.max_modified_chunks);
    chunks.modified_chunk_count = 0;

    chunks.track_history = 1;

    chunks.wait_mesh_update = 0;

    chunks_to_interpolate.max_modified = 40;
    chunks_to_interpolate.modification_count = 0;
    chunks_to_interpolate.modifications = FL_MALLOC(chunk_modifications_t, chunks_to_interpolate.max_modified);
    memset(chunks_to_interpolate.modifications, 0, sizeof(chunk_modifications_t) * chunks_to_interpolate.max_modified);

#if 1
    w_add_sphere_m(vector3_t(20.0f), 40.0f);
    w_add_sphere_m(vector3_t(-40.0f, 40.0f, -40.0f), 20.0f);
    w_add_sphere_m(vector3_t(-70.0f, 90.0f, 45.0f), 25.0f);
#endif

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
    ivector3_t vs_center = w_convert_world_to_voxel(ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = w_convert_voxel_to_chunk(vs_center);

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
                        current_chunk = get_chunk(c);
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

chunk_t *access_chunk(
    const ivector3_t &coord) {
    uint32_t hash = s_hash_chunk_coord(coord);
    uint32_t *index = chunks.chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return chunks.chunks[*index];
    }
    else {
        return NULL;
    }
}

chunk_t *get_chunk(
    const ivector3_t &coord) {
    uint32_t hash = s_hash_chunk_coord(coord);
    uint32_t *index = chunks.chunk_indices.get(hash);
    
    if (index) {
        // Chunk was already added
        return chunks.chunks[*index];
    }
    else {
        uint32_t i = chunks.chunks.add();
        chunk_t *&chunk = chunks.chunks[i];
        chunk = FL_MALLOC(chunk_t, 1);
        w_chunk_init(chunk, i, coord);

        chunks.chunk_indices.insert(hash, i);

        return chunk;
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
            ivector3_t voxel = w_convert_world_to_voxel(current_ray_position);
            ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
            chunk_t *chunk = access_chunk(chunk_coord);

            length += length_step;

            if (chunk) {
                ivector3_t local_voxel_coord = w_convert_voxel_to_local_chunk(voxel);
                if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > surface_level) {

                    break;
                }
            }
        }

        sensors->s[i] = length;
    }
}

terraform_package_t w_cast_terrain_ray(
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
        ivector3_t voxel = w_convert_world_to_voxel(vs_position);
        ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = w_convert_voxel_to_local_chunk(voxel);
            if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > surface_level) {
                package.ray_hit_terrain = 1;
                package.position = vector3_t(voxel);
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
        ivector3_t voxel = w_convert_world_to_voxel(package.position);
        ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (!chunk->flags.made_modification) {
            // Push this chunk onto list of modified chunks
            chunks.modified_chunks[chunks.modified_chunk_count++] = chunk;
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
                            chunk_t *new_chunk = get_chunk(chunk_coord);

                            chunk = new_chunk;

                            if (!chunk->flags.made_modification) {
                                // Push this chunk onto list of modified chunks
                                chunks.modified_chunks[chunks.modified_chunk_count++] = chunk;
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
        ivector3_t voxel = w_convert_world_to_voxel(package.position);
        ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = w_convert_voxel_to_local_chunk(voxel);
            if (chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)] > surface_level) {
                package.ray_hit_terrain = 1;
                package.position = package.position;

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
                                    chunk_t *new_chunk = get_chunk(chunk_coord);

                                    chunk = new_chunk;
                                    chunk->flags.made_modification = 1;
                                    chunk->flags.has_to_update_vertices = 1;

                                    current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                }

                                uint32_t voxel_index = get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z);

                                uint8_t *voxel = &chunk->voxels[voxel_index];
                                float proportion = 1.0f - (distance_squared / radius_squared);

                                int32_t current_voxel_value = (int32_t)*voxel;

                                int32_t new_value = (int32_t)(proportion * coeff * dt * speed) + current_voxel_value;

                                uint8_t voxel_value = 0;
                                    
                                if (new_value > (int32_t)MAX_VOXEL_VALUE_I) {
                                    voxel_value = (int32_t)MAX_VOXEL_VALUE_I;
                                }
                                else if (new_value < 0) {
                                    voxel_value = 0;
                                }
                                else {
                                    voxel_value = (uint8_t)new_value;
                                }

                                *voxel = voxel_value;
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

bool w_terraform(
    terraform_type_t type,
    terraform_package_t package,
    float radius,
    float speed,
    float dt) {
    if (chunks.track_history) {
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

// terraform_package_t w_terraform(
//     terraform_type_t type,
//     const vector3_t &ws_ray_start,
//     const vector3_t &ws_ray_direction,
//     float max_reach,
//     float radius,
//     float speed,
//     float dt,
//     world_t *world) {
//     if (chunks.track_history) {
//         return s_terraform_with_history(
//             type,
//             ws_ray_start,
//             ws_ray_direction,
//             max_reach,
//             radius,
//             speed,
//             dt,
//             world);
//     }
//     else {
//         return s_terraform_without_history(
//             type,
//             ws_ray_start,
//             ws_ray_direction,
//             max_reach,
//             radius,
//             speed,
//             dt,
//             world);
//     }
// }

void w_toggle_mesh_update_wait(
    bool value) {
    chunks.wait_mesh_update = value;
}

void activate_chunk_history(
    chunk_t *chunk) {
    //chunk->history = FL_MALLOC(chunk_history_t, 1);
    chunk->history.modification_count = 0;
    memset(chunk->history.modification_pool, SPECIAL_VALUE, CHUNK_VOXEL_COUNT);
}

void w_begin_ai_training_chunks(
    ai_training_session_t type) {
    for (uint32_t i = 0; i < chunks.chunks.data_count; ++i) {
        chunk_t *c = chunks.chunks[i];
        memset(c->voxels, 0, sizeof(uint8_t) * CHUNK_VOXEL_COUNT);
    }

    switch (type) {
    case ATS_WALKING: {
        for (int32_t z = -32; z < 32; ++z) {
            for (int32_t x = -32; x < 32; ++x) {
                ivector3_t voxel_coord = ivector3_t((float)x, -2.0f, (float)z);
                ivector3_t chunk_coord = w_convert_voxel_to_chunk(voxel_coord);
                chunk_t *chunk = get_chunk(chunk_coord);
                chunk->flags.has_to_update_vertices = 1;
                ivector3_t local_coord = w_convert_voxel_to_local_chunk(voxel_coord);
                uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
                chunk->voxels[index] = 80;
            }
        }
    } break;
    }
}

chunk_t **get_active_chunks(
    uint32_t *count) {
    *count = chunks.chunks.data_count;
    return chunks.chunks.data;
}

chunk_t **get_modified_chunks(
    uint32_t *count) {
    *count = chunks.modified_chunk_count;
    return chunks.modified_chunks;
}

void reset_modification_tracker() {
    for (uint32_t i = 0; i < chunks.modified_chunk_count; ++i) {
        chunk_t *c = chunks.modified_chunks[i];

        c->flags.made_modification = 0;

        for (int32_t v = 0; v < c->history.modification_count; ++v) {
            c->history.modification_pool[c->history.modification_stack[v]] = SPECIAL_VALUE;
        }

        c->history.modification_count = 0;
    }

    chunks.modified_chunk_count = 0;
}

void set_chunk_history_tracker_value(
    bool value) {
    chunks.track_history = value;
}

terraform_package_t *w_get_local_current_terraform_package() {
    return &chunks.local_current_terraform_package;
}
