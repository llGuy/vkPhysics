#include <ux_scene.hpp>
#include "cl_render.hpp"
#include "wd_predict.hpp"
#include <vkph_triangle_table.hpp>

namespace cl {

static object_shaders_t chunk_shaders;

struct chunk_color_data_t {
    vector4_t pointer_position;
    vector4_t pointer_color;
    float pointer_radius;
};

static struct chunk_color_mem_t {
    vk::gpu_buffer_t chunk_color_ubo;
    VkDescriptorSet chunk_color_set;
    chunk_color_data_t data;
} chunk_colors;

static struct tmp_t {
    // These are used temporarily to generate a chunk's mesh
    compressed_chunk_mesh_vertex_t *mesh_vertices;
} tmp;

void init_chunk_render_resources() {
    { // Chunk shaders
        vk::shader_binding_info_t binding_info = {};
        binding_info.binding_count = 1;
        binding_info.binding_descriptions = flmalloc<VkVertexInputBindingDescription>(binding_info.binding_count);

        binding_info.attribute_count = 2;
        binding_info.attribute_descriptions = flmalloc<VkVertexInputAttributeDescription>(binding_info.attribute_count);

        binding_info.binding_descriptions[0].binding = 0;
        binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        binding_info.binding_descriptions[0].stride = sizeof(uint32_t) + sizeof(uint32_t);
    
        binding_info.attribute_descriptions[0].binding = 0;
        binding_info.attribute_descriptions[0].location = 0;
        binding_info.attribute_descriptions[0].format = VK_FORMAT_R32_UINT;
        binding_info.attribute_descriptions[0].offset = 0;

        binding_info.attribute_descriptions[1].binding = 0;
        binding_info.attribute_descriptions[1].location = 1;
        binding_info.attribute_descriptions[1].format = VK_FORMAT_R32_UINT;
        binding_info.attribute_descriptions[1].offset = sizeof(uint32_t);

        const char *albedo_paths[] = {
            "shaders/SPV/chunk_mesh.vert.spv",
            "shaders/SPV/chunk_mesh.geom.spv",
            "shaders/SPV/chunk_mesh.frag.spv"
        };

        const char *shadow_paths[] = {
            "shaders/SPV/chunk_mesh_shadow.vert.spv",
            "shaders/SPV/shadow.frag.spv"
        };

        vk::shader_create_info_t sc_info;
        sc_info.binding_info = &binding_info;
        sc_info.shader_paths = albedo_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sc_info.cull_mode = VK_CULL_MODE_FRONT_BIT;
        sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        chunk_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_STATIC | vk::MT_PASS_EXTRA_UNIFORM_BUFFER);

        sc_info.shader_paths = shadow_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        chunk_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_STATIC);
    }

    { // UBOs used in chunk rendering
        chunk_colors.data.pointer_position = vector4_t(0.0f);
        chunk_colors.data.pointer_color = vector4_t(0.0f);
        chunk_colors.data.pointer_radius = 0.0f;

        chunk_colors.chunk_color_ubo.init(
            sizeof(chunk_color_data_t),
            &chunk_colors.data,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        chunk_colors.chunk_color_set = vk::create_buffer_descriptor_set(
            chunk_colors.chunk_color_ubo.buffer,
            sizeof(chunk_color_data_t),
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    { // Temporary vertices used to create chunk meshes.
        tmp.mesh_vertices = flmalloc<compressed_chunk_mesh_vertex_t>(vkph::CHUNK_MAX_VERTICES_PER_CHUNK);
    }
}

chunk_render_t *init_chunk_render(const vkph::chunk_t *chunk, const vector3_t &ws_position) {
    chunk_render_t *chunk_render = FL_MALLOC(chunk_render_t, 1);

    memset(chunk_render, 0, sizeof(chunk_render_t));

    // Color of each vertex: uint32_t
    uint32_t buffer_size = sizeof(compressed_chunk_mesh_vertex_t) * vkph::CHUNK_MAX_VERTICES_PER_CHUNK;

    chunk_render->mesh.push_buffer(vk::BT_VERTEX);
    vk::mesh_buffer_t *vertex_gpu_buffer = chunk_render->mesh.get_mesh_buffer(vk::BT_VERTEX);
    vertex_gpu_buffer->gpu_buffer.init(
        buffer_size,
        NULL,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    chunk_render->mesh.vertex_offset = 0;
    chunk_render->mesh.vertex_count = 0;
    chunk_render->mesh.first_index = 0;
    chunk_render->mesh.index_offset = 0;
    chunk_render->mesh.index_count = 0;
    chunk_render->mesh.index_type = VK_INDEX_TYPE_UINT32;

    chunk_render->mesh.init_mesh_vbo_final_list();

    chunk_render->render_data.model = glm::translate(ws_position);
    chunk_render->render_data.pbr_info.x = 0.1f;
    chunk_render->render_data.pbr_info.y = 0.1f;
    chunk_render->render_data.color = vector4_t(0.0f);

    return chunk_render;
}

void destroy_chunk_render(chunk_render_t *render) {
    if (render) {
        vk::mesh_buffer_t *mesh_buffer = render->mesh.get_mesh_buffer(vk::BT_VERTEX);
        if (mesh_buffer) {
            vk::destroy_sensitive_buffer(&render->mesh.get_mesh_buffer(vk::BT_VERTEX)->gpu_buffer);
        }
        FL_FREE(render);
        render = NULL;
    }
}

static vkph::voxel_t s_chunk_edge_voxel_value(
    int32_t x,
    int32_t y,
    int32_t z,
    bool *doesnt_exist,
    ivector3_t chunk_coord,
    const vkph::state_t *state) {
    int32_t chunk_coord_offset_x = 0, chunk_coord_offset_y = 0, chunk_coord_offset_z = 0;
    int32_t final_x = x, final_y = y, final_z = z;

    if (x == vkph::CHUNK_EDGE_LENGTH) {
        final_x = 0;
        chunk_coord_offset_x = 1;
    }
    if (y == vkph::CHUNK_EDGE_LENGTH) {
        final_y = 0;
        chunk_coord_offset_y = 1;
    }
    if (z == vkph::CHUNK_EDGE_LENGTH) {
        final_z = 0;
        chunk_coord_offset_z = 1;
    }

    ivector3_t coord = ivector3_t(
        chunk_coord.x + chunk_coord_offset_x,
        chunk_coord.y + chunk_coord_offset_y,
        chunk_coord.z + chunk_coord_offset_z);

    const vkph::chunk_t *chunk_ptr = state->access_chunk(coord);
    
    *doesnt_exist = (bool)(chunk_ptr == nullptr);

    if (*doesnt_exist)
        return { 0, 0 };
    
    return chunk_ptr->voxels[vkph::get_voxel_index(final_x, final_y, final_z)];
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
    vkph::voxel_color_t color,
    uint8_t v0,
    uint8_t v1,
    uncompressed_chunk_mesh_vertex_t *vertices,
    vkph::voxel_t *voxel_values,
    uint8_t surface_level,
    compressed_chunk_mesh_vertex_t *mesh_vertices,
    uint32_t *vertex_count) {
    float surface_level_f = (float)surface_level;
    float voxel_value0 = (float)voxel_values[v0].value;
    float voxel_value1 = (float)voxel_values[v1].value;

    if (voxel_value0 > voxel_value1) {
        float tmp = voxel_value0;
        voxel_value0 = voxel_value1;
        voxel_value1 = tmp;

        uint8_t tmp_v = v0;
        v0 = v1;
        v1 = tmp_v;
    }

    float interpolated_voxel_values = lerp(voxel_value0, voxel_value1, surface_level_f);
    
    vector3_t vertex = interpolate(vertices[v0].position, vertices[v1].position, interpolated_voxel_values);

    vector3_t floor_of_vertex = glm::min(glm::floor(vertex), vector3_t(15.0f));
    ivector3_t ifloor_of_vertex = ivector3_t(floor_of_vertex);

    vector3_t normalized_vertex = (vertex - floor_of_vertex);
    ivector3_t inormalized_vertex = ivector3_t((vertex - floor_of_vertex) * 255.0f);
    
    uint32_t low = 0;
    uint32_t high = 0;

    low += (ifloor_of_vertex.x << 28);
    low += (ifloor_of_vertex.y << 24);
    low += (ifloor_of_vertex.z << 20);
    low += (inormalized_vertex.x << 12);
    low += (inormalized_vertex.y << 4);
    low += inormalized_vertex.z >> 4;

    high += inormalized_vertex.z << 28;
    high += color << 20;

    mesh_vertices[*(vertex_count)].low = low;
    mesh_vertices[*(vertex_count)].high = high;

#if 0
    mesh_vertices[*(vertex_count)].position = vertex;
    // In the shader, we will need to convert this 8-bit color to fully RGB color
    mesh_vertices[*(vertex_count)].color = (uint32_t)color;
#endif

    ++(*vertex_count);
}

#include <triangle_table.inc>

static void s_update_chunk_mesh_voxel_pair(
    vkph::voxel_t *voxel_values,
    uint32_t x,
    uint32_t y,
    uint32_t z,
    uint8_t surface_level,
    compressed_chunk_mesh_vertex_t *mesh_vertices,
    uint32_t *vertex_count) {
    uint8_t bit_combination = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        bool is_over_surface = (voxel_values[i].value > surface_level);
        bit_combination |= is_over_surface << i;
    }

    const int8_t *triangle_entry = &vkph::TRIANGLE_TABLE[bit_combination][0];

    uint32_t edge = 0;

    int8_t edge_pair[3] = {};

    while(triangle_entry[edge] != -1) {
        int8_t edge_index = triangle_entry[edge];
        edge_pair[edge % 3] = edge_index;

        if (edge % 3 == 2) {
            vkph::voxel_color_t color;
            uint32_t dominant_voxel = 0;

            uncompressed_chunk_mesh_vertex_t vertices[8] = {};
            for (uint32_t i = 0; i < 8; ++i) {
                vertices[i].position = NORMALIZED_CUBE_VERTICES[i] + vector3_t(0.5f) + vector3_t((float)x, (float)y, (float)z);
                vertices[i].color = voxel_values[i].color;

                if (voxel_values[i].value > voxel_values[dominant_voxel].value) {
                    dominant_voxel = i;
                }
            }

            color = voxel_values[dominant_voxel].color;

            for (uint32_t i = 0; i < 3; ++i) {
                switch(edge_pair[i]) {
                case 0: { s_push_vertex_to_triangle_array(color, 0, 1, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 1: { s_push_vertex_to_triangle_array(color, 1, 2, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 2: { s_push_vertex_to_triangle_array(color, 2, 3, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 3: { s_push_vertex_to_triangle_array(color, 3, 0, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 4: { s_push_vertex_to_triangle_array(color, 4, 5, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 5: { s_push_vertex_to_triangle_array(color, 5, 6, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 6: { s_push_vertex_to_triangle_array(color, 6, 7, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 7: { s_push_vertex_to_triangle_array(color, 7, 4, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 8: { s_push_vertex_to_triangle_array(color, 0, 4, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 9: { s_push_vertex_to_triangle_array(color, 1, 5, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 10: { s_push_vertex_to_triangle_array(color, 2, 6, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                case 11: { s_push_vertex_to_triangle_array(color, 3, 7, vertices, voxel_values, surface_level, mesh_vertices, vertex_count); } break;
                }
            }
        }

        ++edge;
    }
}

static uint32_t s_generate_chunk_verts(
    uint8_t surface_level,
    const vkph::chunk_t *c,
    compressed_chunk_mesh_vertex_t *mesh_vertices,
    const vkph::state_t *state) {
    uint32_t vertex_count = 0;

    const vkph::chunk_t *x_superior = state->access_chunk(ivector3_t(c->chunk_coord.x + 1, c->chunk_coord.y, c->chunk_coord.z));
    const vkph::chunk_t *y_superior = state->access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y + 1, c->chunk_coord.z));
    const vkph::chunk_t *z_superior = state->access_chunk(ivector3_t(c->chunk_coord.x, c->chunk_coord.y, c->chunk_coord.z + 1));
    
    bool doesnt_exist = 0;
    if (x_superior) {
        // x_superior
        for (uint32_t z = 0; z < vkph::CHUNK_EDGE_LENGTH; ++z) {
            for (uint32_t y = 0; y < vkph::CHUNK_EDGE_LENGTH - 1; ++y) {
                doesnt_exist = 0;
                
                uint32_t x = vkph::CHUNK_EDGE_LENGTH - 1;

                vkph::voxel_t voxel_values[8] = {
                    c->voxels[vkph::get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x]    [y][z + 1],
                    
                    c->voxels[vkph::get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z,&doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, state) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
            }
        }
    }

    if (y_superior) {
        // y_superior    
        for (uint32_t z = 0; z < vkph::CHUNK_EDGE_LENGTH; ++z) {
            for (uint32_t x = 0; x < vkph::CHUNK_EDGE_LENGTH; ++x) {
                doesnt_exist = 0;
                
                uint32_t y = vkph::CHUNK_EDGE_LENGTH - 1;

                vkph::voxel_t voxel_values[8] = {
                    c->voxels[vkph::get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x]    [y][z + 1],
                    
                    s_chunk_edge_voxel_value(x, y + 1, z, &doesnt_exist, c->chunk_coord, state),
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, state) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
               
            }
        }
    }

    if (z_superior) {
        // z_superior
        for (uint32_t y = 0; y < vkph::CHUNK_EDGE_LENGTH - 1; ++y) {
            for (uint32_t x = 0; x < vkph::CHUNK_EDGE_LENGTH - 1; ++x) {
                doesnt_exist = 0;
                
                uint32_t z = vkph::CHUNK_EDGE_LENGTH - 1;

                vkph::voxel_t voxel_values[8] = {
                    c->voxels[vkph::get_voxel_index(x, y, z)],
                    s_chunk_edge_voxel_value(x + 1, y, z, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z],
                    s_chunk_edge_voxel_value(x + 1, y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y][z + 1],
                    s_chunk_edge_voxel_value(x,     y, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x]    [y][z + 1],
                    
                    c->voxels[vkph::get_voxel_index(x, y + 1, z)],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z],
                    s_chunk_edge_voxel_value(x + 1, y + 1, z + 1, &doesnt_exist, c->chunk_coord, state),//voxels[x + 1][y + 1][z + 1],
                    s_chunk_edge_voxel_value(x,     y + 1, z + 1, &doesnt_exist, c->chunk_coord, state) };//voxels[x]    [y + 1][z + 1] };

                if (!doesnt_exist)
                    s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
            }
        }
    }
    
    for (uint32_t z = 0; z < vkph::CHUNK_EDGE_LENGTH - 1; ++z) {
        for (uint32_t y = 0; y < vkph::CHUNK_EDGE_LENGTH - 1; ++y) {
            for (uint32_t x = 0; x < vkph::CHUNK_EDGE_LENGTH - 1; ++x) {
                vkph::voxel_t voxel_values[8] = {
                    c->voxels[vkph::get_voxel_index(x, y, z)],
                    c->voxels[vkph::get_voxel_index(x + 1, y, z)],
                    c->voxels[vkph::get_voxel_index(x + 1, y, z + 1)],
                    c->voxels[vkph::get_voxel_index(x, y, z + 1)],
                    
                    c->voxels[vkph::get_voxel_index(x, y + 1, z)],
                    c->voxels[vkph::get_voxel_index(x + 1, y + 1, z)],
                    c->voxels[vkph::get_voxel_index(x + 1, y + 1, z + 1)],
                    c->voxels[vkph::get_voxel_index(x, y + 1, z + 1)] };

                s_update_chunk_mesh_voxel_pair(voxel_values, x, y, z, surface_level, mesh_vertices, &vertex_count);
            }
        }
    }

    return vertex_count;
}

void update_chunk_gpu_resources(
    VkCommandBuffer command_buffer,
    uint8_t surface_level,
    vkph::chunk_t *c,
    compressed_chunk_mesh_vertex_t *mesh_vertices,
    const vkph::state_t *state) {
    if (!mesh_vertices) {
        mesh_vertices = tmp.mesh_vertices;
    }

    uint32_t vertex_count = s_generate_chunk_verts(surface_level, c, mesh_vertices, state);
    if (vertex_count) {
        c->flags.active_vertices = 1;

        if (!c->render) {
            c->render = init_chunk_render(c, vkph::space_chunk_to_world(c->chunk_coord));
        }

        static const uint32_t MAX_UPDATE_BUFFER_SIZE = 65536;
        uint32_t update_size = vertex_count * sizeof(compressed_chunk_mesh_vertex_t);

        uint32_t loop_count = update_size / MAX_UPDATE_BUFFER_SIZE;

        typedef char copy_byte_t;
        copy_byte_t *pointer = (copy_byte_t *)mesh_vertices;
        uint32_t to_copy_left = update_size;
        uint32_t copied = 0;
        for (uint32_t i = 0; i < loop_count; ++i) {
            c->render->mesh.get_mesh_buffer(vk::BT_VERTEX)->gpu_buffer.update(
                command_buffer,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                copied,
                MAX_UPDATE_BUFFER_SIZE,
                pointer);

            copied += MAX_UPDATE_BUFFER_SIZE;
            pointer += MAX_UPDATE_BUFFER_SIZE;
            to_copy_left -= MAX_UPDATE_BUFFER_SIZE;
        }

        if (to_copy_left) {
            c->render->mesh.get_mesh_buffer(vk::BT_VERTEX)->gpu_buffer.update(
                command_buffer,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                copied,
                to_copy_left,
                pointer);
        }

        c->render->mesh.vertex_count = vertex_count;
    }
    else {
        c->flags.active_vertices = 0;

        if (c->render) {
            destroy_chunk_render(c->render);
            c->render = NULL;
            LOG_INFO("Destroyed chunk data\n");
        }
    }
}

void chunks_gpu_sync_and_render(
    struct frustum_t *frustum,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state) {
    const uint32_t max_chunks_loaded_per_frame = 10;
    uint32_t chunks_loaded = 0;

    vkph::terraform_package_t *current_terraform_package = wd_get_local_terraform_package();

    if (current_terraform_package->ray_hit_terrain) {
        chunk_colors.data.pointer_radius = 3.0f;
    }
    else {
        chunk_colors.data.pointer_radius = 0.0f;
    }

    chunk_colors.data.pointer_position = vector4_t(current_terraform_package->ws_contact_point, 1.0f);
    chunk_colors.data.pointer_color = vector4_t(0.0f, 1.0f, 1.0f, 1.0f);

    chunk_colors.chunk_color_ubo.update(
        transfer_command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(chunk_color_data_t),
        &chunk_colors.data);

    vk::begin_mesh_submission(
        render_command_buffer,
        &chunk_shaders.albedo,
        chunk_colors.chunk_color_set);

    uint32_t chunk_count;
    vkph::chunk_t **chunks = state->get_active_chunks(&chunk_count);
    uint8_t surface_level = vkph::CHUNK_SURFACE_LEVEL;

    ux::scene_info_t *scene_info = ux::get_scene_info();
    const vk::eye_3d_info_t *eye_info = &scene_info->eye;

    for (uint32_t i = 0; i < chunk_count; ++i) {
        vkph::chunk_t *c = chunks[i];
        if (c) {
            if (c->flags.has_to_update_vertices && chunks_loaded < max_chunks_loaded_per_frame) {
                c->flags.has_to_update_vertices = 0;
                // Update chunk mesh and put on GPU + send to command buffer
                // TODO:
                update_chunk_gpu_resources(
                    transfer_command_buffer,
                    surface_level,
                    c,
                    NULL,
                    state);

                ++chunks_loaded;
            }
        
            if (c->flags.active_vertices) {
                // Make sure that the chunk is in the view of the frustum
                vector3_t chunk_center = vector3_t((c->chunk_coord * 16)) + vector3_t(8.0f);
                vector3_t diff = chunk_center - eye_info->position;
                // if (frustum_check_cube(frustum, chunk_center, 8.0f) || glm::dot(diff, diff) < 32.0f * 32.0f) {
                vk::submit_mesh(
                    render_command_buffer,
                    &c->render->mesh,
                    &chunk_shaders.albedo,
                    {&c->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
                //}

                submit_mesh_shadow(
                    render_shadow_command_buffer,
                    &c->render->mesh,
                    &chunk_shaders.shadow,
                    {&c->render->render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
            }
        }
    }
}

}
