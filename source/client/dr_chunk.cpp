#include "dr_rsc.hpp"
#include "dr_chunk.hpp"
#include "renderer/renderer.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/chunk.hpp>
#include <common/constant.hpp>
#include <common/allocators.hpp>

static uint32_t id = 0;

chunk_render_t *dr_chunk_render_init(const chunk_t *chunk, const vector3_t &ws_position) {
    chunk_render_t *chunk_render = FL_MALLOC(chunk_render_t, 1);

    memset(chunk_render, 0, sizeof(chunk_render_t));

    uint32_t buffer_size = sizeof(vector3_t) * CHUNK_MAX_VERTICES_PER_CHUNK;

    push_buffer_to_mesh(BT_VERTEX, &chunk_render->mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, &chunk_render->mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        buffer_size,
        NULL,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    chunk_render->mesh.vertex_offset = 0;
    chunk_render->mesh.vertex_count = 0;
    chunk_render->mesh.first_index = 0;
    chunk_render->mesh.index_offset = 0;
    chunk_render->mesh.index_count = 0;
    chunk_render->mesh.index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(&chunk_render->mesh);

    chunk_render->render_data.model = glm::translate(ws_position);
    chunk_render->render_data.pbr_info.x = 0.1f;
    chunk_render->render_data.pbr_info.y = 0.1f;
    chunk_render->render_data.color = vector4_t(0.0f);

    chunk_render->id = id;
    id++;

    return chunk_render;
}

void dr_destroy_chunk_render(chunk_render_t *render) {
    if (render) {
        mesh_buffer_t *mesh_buffer = get_mesh_buffer(BT_VERTEX, &render->mesh);
        if (mesh_buffer) {
            destroy_sensitive_gpu_buffer(get_mesh_buffer(BT_VERTEX, &render->mesh)->gpu_buffer);
        }
        FL_FREE(render);
        render = NULL;
    }
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

    ivector3_t coord = ivector3_t(
        chunk_coord.x + chunk_coord_offset_x,
        chunk_coord.y + chunk_coord_offset_y,
        chunk_coord.z + chunk_coord_offset_z);

    chunk_t *chunk_ptr = access_chunk(coord);
    
    *doesnt_exist = (bool)(chunk_ptr == nullptr);

    if (*doesnt_exist)
        return 0;
    
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
    vector3_t(-0.5f, +0.5f, +0.5f)
};

static const ivector3_t NORMALIZED_CUBE_VERTEX_INDICES[8] = {
    ivector3_t(0, 0, 0),
    ivector3_t(1, 0, 0),
    ivector3_t(1, 0, 1),
    ivector3_t(0, 0, 1),
    ivector3_t(0, 1, 0),
    ivector3_t(1, 1, 0),
    ivector3_t(1, 1, 1),
    ivector3_t(0, 1, 1)
};

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

#include <common/triangle_table.inc>

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

uint32_t dr_generate_chunk_verts(uint8_t surface_level, const chunk_t *c, vector3_t *mesh_vertices) {
    if (!mesh_vertices)
        mesh_vertices = dr_get_tmp_mesh_verts();

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

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
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

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
               
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

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
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

                s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
            }
        }
    }

    return vertex_count;
}

void dr_update_chunk_draw_rsc(VkCommandBuffer command_buffer, uint8_t surface_level, chunk_t *c, vector3_t *mesh_vertices) {
    if (!mesh_vertices) {
        mesh_vertices = dr_get_tmp_mesh_verts();
    }

    uint32_t vertex_count = dr_generate_chunk_verts(surface_level, c, mesh_vertices);
    if (vertex_count) {
        c->flags.active_vertices = 1;

        if (!c->render) {
            c->render = dr_chunk_render_init(c, space_chunk_to_world(c->chunk_coord));
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
            dr_destroy_chunk_render(c->render);
            c->render = NULL;
            LOG_INFO("Destroyed chunk data\n");
        }
    }
}
