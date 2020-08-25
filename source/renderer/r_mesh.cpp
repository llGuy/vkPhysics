#include <math.h>
#include <stdio.h>
#include <string.h>
#include "common/log.hpp"
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include <common/tools.hpp>
#include <common/files.hpp>
#include <common/string.hpp>
#include <common/tokeniser.hpp>
#include <common/allocators.hpp>
#include <common/serialiser.hpp>
#include <common/containers.hpp>
#include <vulkan/vulkan_core.h>

const float PI = 3.14159265359f;

void push_buffer_to_mesh(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    mesh->buffer_type_stack[mesh->buffer_count++] = buffer_type;
    mesh->buffers[buffer_type].type = buffer_type;
}

bool mesh_has_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    return mesh->buffers[buffer_type].type == buffer_type;

}

mesh_buffer_t *get_mesh_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    if (mesh_has_buffer(buffer_type, mesh)) {
        return &mesh->buffers[buffer_type];
    }
    else {
        return NULL;
    }
}

shader_binding_info_t create_mesh_binding_info(
    mesh_t *mesh) {
    shader_binding_info_t info = {};
    uint32_t start_index = 0;
    if (mesh_has_buffer(BT_INDICES, mesh)) {
        start_index = 1;
    }

    uint32_t binding_count = mesh->buffer_count - start_index;

    info.binding_count = binding_count;
    info.binding_descriptions = FL_MALLOC(VkVertexInputBindingDescription, info.binding_count);

    // Each binding will store one attribute
    info.attribute_count = binding_count;
    info.attribute_descriptions = FL_MALLOC(VkVertexInputAttributeDescription, info.binding_count);

    for (uint32_t i = 0; start_index < mesh->buffer_count; ++start_index, ++i) {
        VkVertexInputBindingDescription *current_binding = &info.binding_descriptions[i];
        VkVertexInputAttributeDescription *current_attribute = &info.attribute_descriptions[i];

        // These depend on the buffer type
        VkFormat format;
        uint32_t stride;

        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(mesh->buffer_type_stack[start_index], mesh);
        
        switch (current_mesh_buffer->type) {
        case BT_VERTEX: case BT_NORMAL: {
            format = VK_FORMAT_R32G32B32_SFLOAT;
            stride = sizeof(vector3_t);
        } break;
        case BT_UVS: {
            format = VK_FORMAT_R32G32_SFLOAT;
            stride = sizeof(vector2_t);
        } break;
        case BT_JOINT_WEIGHT: {
            format = VK_FORMAT_R32G32B32A32_SFLOAT;
            stride = sizeof(vector4_t);
        } break;
        case BT_JOINT_INDICES: {
            format = VK_FORMAT_R32G32B32A32_SINT;
            stride = sizeof(ivector4_t);
        } break;
        default: {
            LOG_ERRORV("Mesh buffer %d not supported\n", current_mesh_buffer->type);
            exit(1);
        } break;
        }
        
        current_binding->binding = i;
        current_binding->stride = stride;
        current_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        current_attribute->binding = i;
        current_attribute->location = i;
        current_attribute->format = format;
        current_attribute->offset = 0;
    }

    return info;
}

void free_mesh_binding_info(
    shader_binding_info_t *info) {
    FL_FREE(info->attribute_descriptions);
    FL_FREE(info->binding_descriptions);
}

shader_t create_mesh_shader_color(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling,
    VkPrimitiveTopology topology,
    mesh_type_flags_t mesh_type) {
    VkDescriptorType types[] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    uint32_t type_count = 1;
    if (mesh_type & MT_ANIMATED) {
        type_count++;
    }

    if (mesh_type & MT_PASS_EXTRA_UNIFORM_BUFFER) {
        type_count++;
    }

    uint32_t push_constant_size = sizeof(mesh_render_data_t);
    if (mesh_type & MT_MERGED_MESH) {
        push_constant_size += sizeof(matrix4_t) + sizeof(float);
    }

    return create_3d_shader_color(
        binding_info,
        push_constant_size,
        types, type_count,
        shader_paths,
        shader_flags,
        topology,
        culling);
}

shader_t create_mesh_shader_shadow(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    mesh_type_flags_t mesh_type) {
        if (mesh_type == MT_STATIC) {
        VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
        return create_3d_shader_shadow(
            binding_info,
            sizeof(mesh_render_data_t),
            &ubo_type, 1,
            shader_paths,
            shader_flags);
    }
    else {
        VkDescriptorType ubo_types[] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    
        return create_3d_shader_shadow(
            binding_info,
            sizeof(mesh_render_data_t),
            ubo_types, 2,
            shader_paths,
            shader_flags);
    }
}

void create_mesh_vbo_final_list(mesh_t *mesh) {
    uint32_t vb_count = mesh_has_buffer(BT_INDICES, mesh) ? mesh->buffer_count - 1 : mesh->buffer_count;
    mesh->vertex_buffer_count = vb_count;

    uint32_t counter = 0;
    for (uint32_t i = 0; i < mesh->buffer_count; ++i) {
        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(mesh->buffer_type_stack[i], mesh);
        
        if (current_mesh_buffer->type == BT_INDICES) {
            mesh->index_buffer = current_mesh_buffer->gpu_buffer.buffer;
        }
        else {
            mesh->vertex_buffers_final[counter++] = current_mesh_buffer->gpu_buffer.buffer;
        }
    }

    for (uint32_t i = 0; i < vb_count; ++i) {
        mesh->vertex_buffers_offsets[i] = mesh->vertex_offset;
    }
}

static vector3_t *s_load_sphere_vertices(
    uint32_t *vertex_count) {
    
}

static void s_load_sphere(
    mesh_t *mesh,
    shader_binding_info_t *binding_info) {
    int32_t sector_count = 16;
    int32_t stack_count = 16;
    
    uint32_t vertex_count = (sector_count + 1) * (stack_count + 1);
    uint32_t index_count = sector_count * stack_count * 6;
    
    vector3_t *positions = FL_MALLOC(vector3_t, vertex_count);

    //vector3_t *normals = FL_MALLOC(vector3_t, vertex_count);
    // vector2_t *uvs = FL_MALLOC(vector2_t, vertex_count);
    
    uint32_t *indices = FL_MALLOC(uint32_t, index_count);
    
    float sector_step = 2.0f * PI / sector_count;
    float stack_step = PI / stack_count;
    float sector_angle = 0;
    float stack_angle = 0;

    uint32_t counter = 0;
    
    for (int32_t i = 0; i <= stack_count; ++i) {
        stack_angle = PI / 2.0f - i * stack_step;
        float xy = cos(stack_angle);
        float z = sin(stack_angle);

        for (int32_t j = 0; j <= sector_count; ++j) {
            sector_angle = j * sector_step;

            float x = xy * cos(sector_angle);
            float y = xy * sin(sector_angle);

            positions[counter++] = vector3_t(x, z, y);
            // uvs[counter++] = vector2_t((float)j / (float)sector_count, (float)i / (float)stack_count);
        }
    }

    counter = 0;
    
    for (int32_t i = 0; i < stack_count; ++i) {
        int k1 = i * (sector_count + 1);
        int k2 = k1 + sector_count + 1;

        for (int32_t j = 0; j < sector_count; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices[counter++] = k1;
                indices[counter++] = k2;
                indices[counter++] = k1 + 1;
            }

            if (i != (stack_count - 1)) {
                indices[counter++] = k1 + 1;
                indices[counter++] = k2;
                indices[counter++] = k2 + 1;
            }
        }
    }

    index_count = counter;

    push_buffer_to_mesh(BT_INDICES, mesh);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES, mesh);
    indices_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * index_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * vertex_count,
        positions,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // push_buffer_to_mesh(BT_UVS, mesh);
    // mesh_buffer_t *uvs_gpu_buffer = get_mesh_buffer(BT_UVS, mesh);
    // uvs_gpu_buffer->gpu_buffer = create_gpu_buffer(
    //     sizeof(vector2_t) * vertex_count,
    //     uvs,
    //     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (binding_info) {
        *binding_info = create_mesh_binding_info(mesh);
    }

    mesh->vertex_offset = 0;
    mesh->vertex_count = vertex_count;
    mesh->first_index = 0;
    mesh->index_offset = 0;
    mesh->index_count = index_count;
    mesh->index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(mesh);

    FL_FREE(positions);
    
    // FL_FREE(normals);
    // FL_FREE(uvs);
    
    FL_FREE(indices);
}

// TODO:
static void s_load_cube(
    mesh_t *mesh,
    shader_binding_info_t *binding_info) {
    vector3_t vertices[8] = {
        vector3_t(-1.0f, -1.0f, 1.0f),
        vector3_t(1.0f, -1.0f, 1.0f),
        vector3_t(1.0f, 1.0f, 1.0f),
        vector3_t(-1.0f, 1.0f, 1.0f),
        vector3_t(-1.0f, -1.0f, -1.0f),
        vector3_t(1.0f, -1.0f, -1.0f),
        vector3_t(1.0f, 1.0f, -1.0f),
        vector3_t(-1.0f, 1.0f, -1.0f)
    };

    uint32_t indices[36] = {
        0, 1, 2,
        2, 3, 0,

        1, 5, 6,
        6, 2, 1,

        7, 6, 5,
        5, 4, 7,
	    
        3, 7, 4,
        4, 0, 3,
	    
        4, 5, 1,
        1, 0, 4,
	    
        3, 2, 6,
        6, 7, 3
    };

    uint32_t index_count = sizeof(indices) / sizeof(indices[0]);
    uint32_t vertex_count = sizeof(vertices) / sizeof(vertices[0]);

    push_buffer_to_mesh(BT_INDICES, mesh);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES, mesh);
    indices_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * index_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * vertex_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (binding_info) {
        *binding_info = create_mesh_binding_info(mesh);
    }

    mesh->vertex_offset = 0;
    mesh->vertex_count = vertex_count;
    mesh->first_index = 0;
    mesh->index_offset = 0;
    mesh->index_count = index_count;
    mesh->index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(mesh);
}

void load_mesh_internal(
    internal_mesh_type_t mesh_type,
    mesh_t *mesh,
    shader_binding_info_t *info) {
    memset(mesh, 0, sizeof(mesh_t));

    switch (mesh_type) {
    case IM_SPHERE: {
        s_load_sphere(mesh, info);
    } break;
       
    case IM_CUBE: {
        s_load_cube(mesh, info);
    } break;
    }
}

struct header_t {
    uint32_t vertices_count;
    uint32_t normals_count;
    uint32_t tcoords_count;
    uint32_t joint_weights_count;
    uint32_t joint_ids_count;
    // UINT32_T
    uint32_t indices_count;
};

static vector3_t *s_get_vertices(
    uint32_t vertices_count,
    serialiser_t *serialiser,
    vector3_t *p = NULL) {
    vector3_t *vertices;

    if (p) {
        vertices = p;
    }
    else {
        vertices = LN_MALLOC(vector3_t, vertices_count);
    }

    for (uint32_t i = 0; i < vertices_count; ++i) {
        vertices[i] = serialiser->deserialise_vector3();
    }

    return vertices;
}

static vector3_t *s_get_normals(
    uint32_t normals_count,
    serialiser_t *serialiser,
    vector3_t *p = NULL) {
    vector3_t *normals;

    if (p) {
        normals = p;
    }
    else {
        normals = LN_MALLOC(vector3_t, normals_count);
    }

    for (uint32_t i = 0; i < normals_count; ++i) {
        normals[i] = serialiser->deserialise_vector3();
    }

    return normals;
}

static vector2_t *s_get_tcoords(
    uint32_t tcoords_count,
    serialiser_t *serialiser,
    vector2_t *p = NULL) {
    vector2_t *tcoords;

    if (p) {
        tcoords = p;
    }
    else {
        tcoords = LN_MALLOC(vector2_t, tcoords_count);
    }

    for (uint32_t i = 0; i < tcoords_count; ++i) {
        tcoords[i].x = serialiser->deserialise_float32();
        tcoords[i].y = serialiser->deserialise_float32();
    }

    return tcoords;
}

static vector4_t *s_get_joint_weights(
    uint32_t joint_weights_count,
    serialiser_t *serialiser,
    vector4_t *p = NULL) {
    vector4_t *joint_weights;

    if (p) {
        joint_weights = p;
    }
    else {
        joint_weights = LN_MALLOC(vector4_t, joint_weights_count);
    }

    for (uint32_t i = 0; i < joint_weights_count; ++i) {
        joint_weights[i].x = serialiser->deserialise_float32();
        joint_weights[i].y = serialiser->deserialise_float32();
        joint_weights[i].z = serialiser->deserialise_float32();
        joint_weights[i].w = serialiser->deserialise_float32();
        float sum = joint_weights[i].x + joint_weights[i].y + joint_weights[i].z + joint_weights[i].w;
        float q = 1.0f / sum;
        joint_weights[i] *= q;
    }

    return joint_weights;
}

static ivector4_t *s_get_joint_ids(
    uint32_t joint_ids_count,
    serialiser_t *serialiser,
    ivector4_t *p = NULL) {
    ivector4_t *joint_ids;

    if (p) {
        joint_ids = p;
    }
    else {
        joint_ids = LN_MALLOC(ivector4_t, joint_ids_count);
    }

    for (uint32_t i = 0; i < joint_ids_count; ++i) {
        joint_ids[i].x = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].y = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].z = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].w = (int32_t)serialiser->deserialise_uint32();
    }

    return joint_ids;
}

static uint32_t *s_get_indices(
    uint32_t indices_count,
    serialiser_t *serialiser) {
    uint32_t *indices = LN_MALLOC(uint32_t, indices_count);

    for (uint32_t i = 0; i < indices_count; ++i) {
        indices[i] = serialiser->deserialise_uint32();
    }

    return indices;
}

void load_mesh_external(
    mesh_t *mesh,
    shader_binding_info_t *binding_info,
    const char *path) {
    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);
    
    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    header_t file_header = {};
    file_header.vertices_count = serialiser.deserialise_uint32();
    file_header.normals_count = serialiser.deserialise_uint32();
    file_header.tcoords_count = serialiser.deserialise_uint32();
    file_header.joint_weights_count = serialiser.deserialise_uint32();
    file_header.joint_ids_count = serialiser.deserialise_uint32();
    file_header.indices_count = serialiser.deserialise_uint32();

    vector3_t *vertices = s_get_vertices(file_header.vertices_count, &serialiser);
    vector3_t *normals = s_get_normals(file_header.normals_count, &serialiser);
    vector2_t *tcoords = s_get_tcoords(file_header.tcoords_count, &serialiser);
    vector4_t *joint_weights = s_get_joint_weights(file_header.joint_weights_count, &serialiser);
    ivector4_t *joint_ids = s_get_joint_ids(file_header.joint_ids_count, &serialiser);
    uint32_t *indices = s_get_indices(file_header.indices_count, &serialiser);

    push_buffer_to_mesh(BT_INDICES, mesh);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES, mesh);
    indices_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * file_header.indices_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * file_header.vertices_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_NORMAL, mesh);
    mesh_buffer_t *normal_gpu_buffer = get_mesh_buffer(BT_NORMAL, mesh);
    normal_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * file_header.normals_count,
        normals,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (file_header.joint_weights_count) {
        push_buffer_to_mesh(BT_JOINT_WEIGHT, mesh);
        mesh_buffer_t *weights_gpu_buffer = get_mesh_buffer(BT_JOINT_WEIGHT, mesh);
        weights_gpu_buffer->gpu_buffer = create_gpu_buffer(
            sizeof(vector4_t) * file_header.joint_weights_count,
            joint_weights,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    
    if (file_header.joint_ids_count) {
        push_buffer_to_mesh(BT_JOINT_INDICES, mesh);
        mesh_buffer_t *joints_gpu_buffer = get_mesh_buffer(BT_JOINT_INDICES, mesh);
        joints_gpu_buffer->gpu_buffer = create_gpu_buffer(
            sizeof(ivector4_t) * file_header.joint_ids_count,
            joint_ids,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    if (binding_info) {
        *binding_info = create_mesh_binding_info(mesh);
    }

    mesh->vertex_offset = 0;
    mesh->vertex_count = file_header.vertices_count;
    mesh->first_index = 0;
    mesh->index_offset = 0;
    mesh->index_count = file_header.indices_count;
    mesh->index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(mesh);
}

void create_player_merged_mesh(
    mesh_t *dst_a, shader_binding_info_t *sbi_a,
    mesh_t *dst_b, shader_binding_info_t *sbi_b,
    mesh_t *dst_merged, shader_binding_info_t *sbi_merged) {
    const char *dude_mesh_path = "assets/models/player.mesh";

    file_handle_t file_handle = create_file(dude_mesh_path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);
    
    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    header_t file_header = {};
    file_header.vertices_count = serialiser.deserialise_uint32();
    file_header.normals_count = serialiser.deserialise_uint32();
    file_header.tcoords_count = serialiser.deserialise_uint32();
    file_header.joint_weights_count = serialiser.deserialise_uint32();
    file_header.joint_ids_count = serialiser.deserialise_uint32();
    file_header.indices_count = serialiser.deserialise_uint32();

    int32_t sector_count = 8;
    int32_t stack_count = 8;
    
    uint32_t sphere_vertex_count = (sector_count + 1) * (stack_count + 1);
    uint32_t sphere_index_count = sector_count * stack_count * 6;

    vector3_t *vertices = LN_MALLOC(vector3_t, file_header.vertices_count + sphere_vertex_count);
    vector3_t *normals = LN_MALLOC(vector3_t, file_header.normals_count + sphere_vertex_count);

    // Won't be used
    vector2_t *tcoords = LN_MALLOC(vector2_t, file_header.tcoords_count + sphere_vertex_count);

    vector4_t *joint_weights = LN_MALLOC(vector4_t, file_header.joint_weights_count + sphere_vertex_count);
    ivector4_t *joint_ids = LN_MALLOC(ivector4_t, file_header.joint_ids_count + sphere_vertex_count);

    uint32_t total_indices_count = MAX(file_header.indices_count, sphere_index_count) * 2;

    uint32_t *indices = LN_MALLOC(uint32_t, total_indices_count);

    uint32_t *dude_indices = NULL;
    uint32_t *sphere_indices = LN_MALLOC(uint32_t, sphere_index_count * 2);

    s_get_vertices(file_header.vertices_count, &serialiser, vertices);
    s_get_normals(file_header.normals_count, &serialiser, normals);
    s_get_tcoords(file_header.tcoords_count, &serialiser, tcoords);
    s_get_joint_weights(file_header.joint_weights_count, &serialiser, joint_weights);
    s_get_joint_ids(file_header.joint_ids_count, &serialiser, joint_ids);
    dude_indices = s_get_indices(file_header.indices_count, &serialiser);

    float sector_step = 2.0f * PI / sector_count;
    float stack_step = PI / stack_count;
    float sector_angle = 0;
    float stack_angle = 0;

    uint32_t counter = file_header.vertices_count;
    
    for (int32_t i = 0; i <= stack_count; ++i) {
        stack_angle = PI / 2.0f - i * stack_step;
        float xy = cos(stack_angle);
        float z = sin(stack_angle);

        for (int32_t j = 0; j <= sector_count; ++j) {
            sector_angle = j * sector_step;

            float x = xy * cos(sector_angle);
            float y = xy * sin(sector_angle);

            vertices[counter++] = vector3_t(x, z, y);
        }
    }

    counter = 0;

    memset(normals + file_header.normals_count, 0, sizeof(vector3_t) * sphere_vertex_count);
    memset(joint_weights + file_header.joint_weights_count, 0, sizeof(vector4_t) * sphere_vertex_count);
    memset(joint_ids + file_header.joint_ids_count, 0, sizeof(ivector4_t) * sphere_vertex_count);

    for (int32_t i = 0; i < stack_count; ++i) {
        int k1 = i * (sector_count + 1);
        int k2 = k1 + sector_count + 1;

        for (int32_t j = 0; j < sector_count; ++j, ++k1, ++k2) {
            if (i != 0) {
                sphere_indices[counter++] = k1;
                sphere_indices[counter++] = k2;
                sphere_indices[counter++] = k1 + 1;
            }

            if (i != (stack_count - 1)) {
                sphere_indices[counter++] = k1 + 1;
                sphere_indices[counter++] = k2;
                sphere_indices[counter++] = k2 + 1;
            }
        }
    }

    sphere_index_count = counter;

    // Now, alternate every 3 indices for the final buffer
    for (uint32_t i = 0; i < total_indices_count; i += 6) {
        uint32_t dude_src_index_start = i / 2;
        uint32_t sphere_src_index_start = i / 2;

        if (sphere_src_index_start >= sphere_index_count) {
            indices[i + 3] = dude_indices[0];
            indices[i + 4] = dude_indices[1];
            indices[i + 5] = dude_indices[2];
            // indices[i + 3] = sphere_indices[0] + file_header.vertices_count;
            // indices[i + 4] = sphere_indices[0 + 1] + file_header.vertices_count;
            // indices[i + 5] = sphere_indices[0 + 2] + file_header.vertices_count;
        }
        else {
            indices[i + 3] = sphere_indices[sphere_src_index_start] + file_header.vertices_count;
            indices[i + 4] = sphere_indices[sphere_src_index_start + 1] + file_header.vertices_count;
            indices[i + 5] = sphere_indices[sphere_src_index_start + 2] + file_header.vertices_count;
        }

        if (dude_src_index_start >= file_header.indices_count) {
            indices[i] = sphere_indices[0];
            indices[i + 1] = sphere_indices[1];
            indices[i + 2] = sphere_indices[2];
        }
        else {
            indices[i] = dude_indices[dude_src_index_start];
            indices[i + 1] = dude_indices[dude_src_index_start + 1];
            indices[i + 2] = dude_indices[dude_src_index_start + 2];
        }
    }

    // Create 3 meshes
    push_buffer_to_mesh(BT_INDICES, dst_merged);
    mesh_buffer_t *merged_indices_buffer = get_mesh_buffer(BT_INDICES, dst_merged);
    merged_indices_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * total_indices_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, dst_merged);
    mesh_buffer_t *merged_vertex_buffer = get_mesh_buffer(BT_VERTEX, dst_merged);
    merged_vertex_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * (file_header.vertices_count + sphere_vertex_count),
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_NORMAL, dst_merged);
    mesh_buffer_t *merged_normal_buffer = get_mesh_buffer(BT_NORMAL, dst_merged);
    merged_normal_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * (file_header.normals_count + sphere_vertex_count),
        normals,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_JOINT_WEIGHT, dst_merged);
    mesh_buffer_t *weights_gpu_buffer = get_mesh_buffer(BT_JOINT_WEIGHT, dst_merged);
    weights_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector4_t) * file_header.joint_weights_count + sphere_vertex_count,
        joint_weights,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
    push_buffer_to_mesh(BT_JOINT_INDICES, dst_merged);
    mesh_buffer_t *joints_gpu_buffer = get_mesh_buffer(BT_JOINT_INDICES, dst_merged);
    joints_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(ivector4_t) * file_header.joint_ids_count + sphere_vertex_count,
        joint_ids,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    dst_merged->vertex_offset = 0;
    dst_merged->vertex_count = file_header.vertices_count + sphere_vertex_count;
    dst_merged->first_index = 0;
    dst_merged->index_offset = 0;
    dst_merged->index_count = total_indices_count;
    dst_merged->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_merged = create_mesh_binding_info(dst_merged);
    create_mesh_vbo_final_list(dst_merged);

    // Create player mesh
    push_buffer_to_mesh(BT_INDICES, dst_a);
    mesh_buffer_t *a_indices_buffer = get_mesh_buffer(BT_INDICES, dst_a);
    a_indices_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * file_header.indices_count,
        dude_indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, dst_a);
    mesh_buffer_t *a_vertex_buffer = get_mesh_buffer(BT_VERTEX, dst_a);
    a_vertex_buffer->gpu_buffer = merged_vertex_buffer->gpu_buffer;

    push_buffer_to_mesh(BT_NORMAL, dst_a);
    mesh_buffer_t *a_normal_buffer = get_mesh_buffer(BT_NORMAL, dst_a);
    a_normal_buffer->gpu_buffer = merged_normal_buffer->gpu_buffer;

    push_buffer_to_mesh(BT_JOINT_WEIGHT, dst_a);
    mesh_buffer_t *a_joint_weight_buffer = get_mesh_buffer(BT_JOINT_WEIGHT, dst_a);
    a_joint_weight_buffer->gpu_buffer = weights_gpu_buffer->gpu_buffer;

    push_buffer_to_mesh(BT_JOINT_INDICES, dst_a);
    mesh_buffer_t *a_joint_indices_buffer = get_mesh_buffer(BT_JOINT_INDICES, dst_a);
    a_joint_indices_buffer->gpu_buffer = joints_gpu_buffer->gpu_buffer;

    dst_a->vertex_offset = 0;
    dst_a->vertex_count = file_header.vertices_count;
    dst_a->first_index = 0;
    dst_a->index_offset = 0;
    dst_a->index_count = file_header.indices_count;
    dst_a->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_a = create_mesh_binding_info(dst_a);
    create_mesh_vbo_final_list(dst_a);

    // Create ball mesh
    push_buffer_to_mesh(BT_INDICES, dst_b);
    mesh_buffer_t *b_indices_buffer = get_mesh_buffer(BT_INDICES, dst_b);
    b_indices_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * sphere_index_count,
        sphere_indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, dst_b);
    mesh_buffer_t *b_vertex_buffer = get_mesh_buffer(BT_VERTEX, dst_b);
    b_vertex_buffer->gpu_buffer = merged_vertex_buffer->gpu_buffer;

    dst_b->vertex_offset = sizeof(vector3_t) * file_header.vertices_count;
    dst_b->vertex_count = sphere_vertex_count;
    dst_b->first_index = 0;
    dst_b->index_offset = 0;
    dst_b->index_count = sphere_index_count;
    dst_b->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_b = create_mesh_binding_info(dst_b);
    create_mesh_vbo_final_list(dst_b);
}

void load_skeleton(
    skeleton_t *skeleton,
    const char *path) {
    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    skeleton->joint_count = serialiser.deserialise_uint32();
    skeleton->joints = FL_MALLOC(joint_t, skeleton->joint_count);
    for (uint32_t i = 0; i < skeleton->joint_count; ++i) {
        joint_t *joint = &skeleton->joints[i];
        joint->joint_id = serialiser.deserialise_uint32();
        joint->joint_name = create_fl_string(serialiser.deserialise_string());

        for (uint32_t c = 0; c < 4; ++c) {
            joint->inverse_bind_transform[c][0] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][1] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][2] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][3] = serialiser.deserialise_float32();
        }

        joint->children_count = serialiser.deserialise_uint32();

        assert(joint->children_count < MAX_CHILD_JOINTS);

        for (uint32_t c = 0; c < joint->children_count; ++c) {
            joint->children_ids[c] = serialiser.deserialise_uint32();
        }
    }
}

// The same linker will be used for al animations
static hash_table_t<uint32_t, 25, 5, 5> animation_name_linker;

static uint32_t s_skip_whitespaces(
    uint32_t i,
    token_t *tokens) {
    for (;; ++i) {
        if (tokens[i].type != TT_WHITESPACE) {
            return i;
        }
    }

    assert(0);
}

struct animation_link_data_t {
    uint32_t animation_count;
    float scale;
    vector3_t rotation;
};

static animation_link_data_t s_fill_animation_linker(
    const char *linker_path) {
    animation_link_data_t data = {};

    data.scale = 1.0f;
    data.rotation = vector3_t(0.0f);

    animation_name_linker.clear();
    animation_name_linker.init();

    uint32_t keyword_ids[] = {
        100,
        101,
        102,
        103,
        104,
        105
    };

    const char *keywords[] = {
        "link",
        "scale",
        "rotate",
        "x",
        "y",
        "z"
    };

    enum {
        KEYWORD_LINK,
        KEYWORD_SCALE,
        KEYWORD_ROTATE,
        X_COMPONENT,
        Y_COMPONENT,
        Z_COMPONENT,
        INVALID_KEYWORD
    };

    tokeniser_init(
        keyword_ids,
        keywords,
        INVALID_KEYWORD);

    file_handle_t file_handle = create_file(linker_path, FLF_NONE);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);
    
    uint32_t token_count = 0;

    token_t *tokens = tokenise(
        (char *)contents.data,
        '#',
        &token_count);

    union {
        struct {
            uint32_t in_comment: 1;
            uint32_t link: 1;
            uint32_t scale: 1;
            uint32_t rotate: 1;
        };

        uint32_t bits;
    } flags = {};

    for (uint32_t i = 0; i < token_count; ++i) {
        if (tokens[i].type == TT_NEW_LINE) {
            flags.in_comment = 0;
        }
        else if (!flags.in_comment) {
            if (tokens[i].type == TT_COMMENT) {
                // Ignore until new line
                flags.in_comment = 1;
            }
            else {
                if (tokens[i].type == keyword_ids[KEYWORD_LINK]) {
                    flags.bits = 0;
                    flags.link = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *dst_id = &tokens[i];
                    uint32_t animation_id = str_to_int(tokens[i].at, tokens[i].length);

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    // Next token should be double quote
                    if (tokens[i].type != TT_DOUBLE_QUOTE) {
                        LOG_ERROR("Error double quote missing\n");
                    }

                    ++i;

                    char *name = tokens[i].at;
                    uint32_t name_length = 0;
                    for (; tokens[i].type != TT_DOUBLE_QUOTE; ++i) {
                        name_length += tokens[i].length;
                    }

                    i = s_skip_whitespaces(i, tokens) - 1; // -1 because of for loop increment

                    animation_name_linker.insert(simple_string_hash(name, name_length), animation_id);

                    ++data.animation_count;
                }
                else if (tokens[i].type == keyword_ids[KEYWORD_SCALE]) {
                    flags.bits = 0;
                    flags.scale = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *scale_value_s = &tokens[i];

                    data.scale = str_to_float(tokens[i].at, tokens[i].length);

                    ++i;

                    i = s_skip_whitespaces(i, tokens);
                }
                else if (tokens[i].type == keyword_ids[KEYWORD_ROTATE]) {
                    flags.bits = 0;
                    flags.rotate = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *component = &tokens[i];

                    // Get value
                    ++i;
                    i = s_skip_whitespaces(i, tokens);

                    token_t *rotation_value_tk = &tokens[i];
                    float rotation_value = str_to_float(rotation_value_tk->at, rotation_value_tk->length);

                    if (component->type == keyword_ids[X_COMPONENT]) {
                        data.rotation.x = rotation_value;
                    }
                    else if (component->type == keyword_ids[Y_COMPONENT]) {
                        data.rotation.y = rotation_value;
                    }
                    else if (component->type == keyword_ids[Z_COMPONENT]) {
                        data.rotation.z = rotation_value;
                    }

                    ++i;
                    i = s_skip_whitespaces(i, tokens);
                }
            }
        }
    }

    return data;
}

void load_animation_cycles(
    animation_cycles_t *cycles,
    const char *linker_path,
    const char *path) {
    animation_link_data_t linked_animations = s_fill_animation_linker(linker_path);

    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    cycles->cycle_count = serialiser.deserialise_uint32();
    cycles->cycle_count = MAX(linked_animations.animation_count, cycles->cycle_count);
    cycles->cycles = FL_MALLOC(animation_cycle_t, cycles->cycle_count);

    for (uint32_t i = 0; i < cycles->cycle_count; ++i) {
        const char *animation_name = create_fl_string(serialiser.deserialise_string());

        uint32_t *animation_id_p = animation_name_linker.get(simple_string_hash(animation_name));
        if (animation_id_p) {
            uint32_t animation_id = *animation_id_p;

            animation_cycle_t *cycle = &cycles->cycles[*animation_id_p];
            cycle->animation_name = animation_name;
            cycle->duration = serialiser.deserialise_float32();
            cycle->joint_animation_count = serialiser.deserialise_uint32();
        
            cycle->joint_animations = FL_MALLOC(joint_key_frames_t, cycle->joint_animation_count);

            for (uint32_t j = 0; j < cycle->joint_animation_count; ++j) {
                joint_key_frames_t *key_frames = &cycle->joint_animations[j];
                key_frames->position_count = serialiser.deserialise_uint32();
                key_frames->rotation_count = serialiser.deserialise_uint32();
                key_frames->scale_count = serialiser.deserialise_uint32();

                key_frames->positions = FL_MALLOC(joint_position_key_frame_t, key_frames->position_count);
                key_frames->rotations = FL_MALLOC(joint_rotation_key_frame_t, key_frames->rotation_count);
                key_frames->scales = FL_MALLOC(joint_scale_key_frame_t, key_frames->scale_count);

                for (uint32_t position = 0; position < key_frames->position_count; ++position) {
                    key_frames->positions[position].time_stamp = serialiser.deserialise_float32();
                    key_frames->positions[position].position = serialiser.deserialise_vector3();
                }

                for (uint32_t rotation = 0; rotation < key_frames->rotation_count; ++rotation) {
                    key_frames->rotations[rotation].time_stamp = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.w = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.x = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.y = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.z = serialiser.deserialise_float32();
                }

                for (uint32_t scale = 0; scale < key_frames->scale_count; ++scale) {
                    key_frames->scales[scale].time_stamp = serialiser.deserialise_float32();
                    key_frames->scales[scale].scale = serialiser.deserialise_vector3();
                }
            }
        }
        else {
            float duration = serialiser.deserialise_float32();
            uint32_t joint_animation_count = serialiser.deserialise_uint32();
        
            for (uint32_t j = 0; j < joint_animation_count; ++j) {
                uint32_t position_count = serialiser.deserialise_uint32();
                uint32_t rotation_count = serialiser.deserialise_uint32();
                uint32_t scale_count = serialiser.deserialise_uint32();

                for (uint32_t position = 0; position < position_count; ++position) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_vector3();
                }

                for (uint32_t rotation = 0; rotation < rotation_count; ++rotation) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                }

                for (uint32_t scale = 0; scale < scale_count; ++scale) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_vector3();
                }
            }
        }
    }

    // Create rotation and scale matrices
    matrix4_t rotation_matrix = matrix4_t(1.0f);
    if (linked_animations.rotation.x != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.x), vector3_t(1.0f, 0.0f, 0.0f));
    }
    if (linked_animations.rotation.y != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.y), vector3_t(0.0f, 1.0f, 0.0f));
    }
    if (linked_animations.rotation.z != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.z), vector3_t(0.0f, 1.0f, 0.0f));
    }

    matrix4_t scale_matrix = glm::scale(vector3_t(linked_animations.scale));

    cycles->scale = scale_matrix;
    cycles->rotation = rotation_matrix;
}

void animated_instance_init(
    animated_instance_t *instance,
    skeleton_t *skeleton,
    animation_cycles_t *cycles) {
    memset(instance, 0, sizeof(animated_instance_t));
    instance->current_animation_time = 0.0f;
    instance->is_interpolating_between_cycles = 0;
    instance->skeleton = skeleton;
    instance->next_bound_cycle = 0;
    instance->interpolated_transforms = FL_MALLOC(matrix4_t, skeleton->joint_count);
    instance->current_positions = FL_MALLOC(vector3_t, skeleton->joint_count);
    instance->current_rotations = FL_MALLOC(quaternion_t, skeleton->joint_count);
    instance->current_scales = FL_MALLOC(vector3_t, skeleton->joint_count);
    instance->current_position_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->current_rotation_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->current_scale_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->cycles = cycles;
    instance->in_between_interpolation_time = 0.2f;

    memset(instance->current_position_indices, 0, sizeof(uint32_t) * skeleton->joint_count);
    memset(instance->current_rotation_indices, 0, sizeof(uint32_t) * skeleton->joint_count);
    memset(instance->current_scale_indices, 0, sizeof(uint32_t) * skeleton->joint_count);

    for (uint32_t i = 0; i < skeleton->joint_count; ++i) {
        instance->interpolated_transforms[i] = matrix4_t(1.0f);
    }

    instance->interpolated_transforms_ubo = create_gpu_buffer(
        sizeof(matrix4_t) * skeleton->joint_count,
        instance->interpolated_transforms,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    instance->descriptor_set = create_buffer_descriptor_set(
        instance->interpolated_transforms_ubo.buffer,
        sizeof(matrix4_t) * skeleton->joint_count,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

static void s_calculate_in_between_final_offsets(
    animated_instance_t *instance,
    uint32_t joint_index,
    matrix4_t parent_transforms) {
    matrix4_t local_transform = instance->interpolated_transforms[joint_index];

    matrix4_t model_transform = parent_transforms * local_transform;

    joint_t *joint_data = &instance->skeleton->joints[joint_index];
    for (uint32_t child = 0; child < joint_data->children_count; ++child) {
        s_calculate_in_between_final_offsets(instance, joint_data->children_ids[child], model_transform);
    }

    instance->interpolated_transforms[joint_index] = model_transform * joint_data->inverse_bind_transform;
}

static void s_calculate_in_between_bone_space_transforms(
    float progression,
    animated_instance_t *instance,
    animation_cycle_t *next) {
    for (uint32_t joint_index = 0; joint_index < instance->skeleton->joint_count; ++joint_index) {
        vector3_t *current_position = &instance->current_positions[joint_index];
        quaternion_t *current_rotation = &instance->current_rotations[joint_index];
        vector3_t *current_scale = &instance->current_scales[joint_index];

        joint_key_frames_t *next_key_frames = &next->joint_animations[joint_index];

        joint_position_key_frame_t *next_position_kf = &next_key_frames->positions[0];
        joint_rotation_key_frame_t *next_rotation_kf = &next_key_frames->rotations[0];

        vector3_t next_position = next_position_kf->position;
        quaternion_t next_rotation = next_rotation_kf->rotation;

        vector3_t new_position = *current_position + progression * (next_position - *current_position);
        quaternion_t new_rotation = glm::slerp(*current_rotation, next_rotation, progression);

        // For now don't interpolate scale
        *current_scale = vector3_t(1.0f);

        // For now, store here
        instance->interpolated_transforms[joint_index] = glm::translate(new_position) * glm::mat4_cast(new_rotation) * glm::scale(*current_scale);
    }
}

static void s_calculate_bone_space_transforms(
    animated_instance_t *instance,
    animation_cycle_t *bound_cycle,
    float current_time) {
    for (uint32_t joint_index = 0; joint_index < instance->skeleton->joint_count; ++joint_index) {
        vector3_t *current_position = &instance->current_positions[joint_index];
        quaternion_t *current_rotation = &instance->current_rotations[joint_index];
        vector3_t *current_scale = &instance->current_scales[joint_index];

        joint_key_frames_t *current_joint_keyframes = &bound_cycle->joint_animations[joint_index];

        for (
            uint32_t p = instance->current_position_indices[joint_index];
            p < current_joint_keyframes->position_count;
            ++p) {
            if (current_joint_keyframes->positions[p].time_stamp > current_time) {
                uint32_t next_position_index = p;
                uint32_t prev_position_index = p - 1;

                joint_position_key_frame_t *prev = &current_joint_keyframes->positions[prev_position_index];
                joint_position_key_frame_t *next = &current_joint_keyframes->positions[next_position_index];
                float position_progression = (current_time - prev->time_stamp) / (next->time_stamp - prev->time_stamp);
                *current_position = prev->position + position_progression * (next->position - prev->position);

                instance->current_position_indices[joint_index] = prev_position_index;

                break;
            }
        }

        for (
            uint32_t r = instance->current_rotation_indices[joint_index];
            r < current_joint_keyframes->rotation_count;
            ++r) {
            if (current_joint_keyframes->rotations[r].time_stamp > current_time) {
                uint32_t next_rotation_index = r;
                uint32_t prev_rotation_index = r - 1;

                joint_rotation_key_frame_t *prev = &current_joint_keyframes->rotations[prev_rotation_index];
                joint_rotation_key_frame_t *next = &current_joint_keyframes->rotations[next_rotation_index];
                float rotation_progression = (current_time - prev->time_stamp) / (next->time_stamp - prev->time_stamp);
                *current_rotation = glm::slerp(prev->rotation, next->rotation, rotation_progression);

                instance->current_rotation_indices[joint_index] = prev_rotation_index;

                break;
            }
        }

        // For now don't interpolate scale
        *current_scale = vector3_t(1.0f);
    }
}

static void s_calculate_final_offset(
    animated_instance_t *instance,
    uint32_t joint_index,
    matrix4_t parent_transform) {
    vector3_t *current_position = &instance->current_positions[joint_index];
    quaternion_t *current_rotation = &instance->current_rotations[joint_index];
    vector3_t *current_scale = &instance->current_scales[joint_index];

    matrix4_t local_transform = glm::translate(*current_position) * glm::mat4_cast(*current_rotation) * glm::scale(*current_scale);

    matrix4_t model_transform = parent_transform * local_transform;

    joint_t *joint_data = &instance->skeleton->joints[joint_index];
    for (uint32_t child = 0; child < joint_data->children_count; ++child) {
        s_calculate_final_offset(instance, joint_data->children_ids[child], model_transform);
    }

    instance->interpolated_transforms[joint_index] = model_transform * joint_data->inverse_bind_transform;
}

void interpolate_joints(
    animated_instance_t *instance,
    float dt,
    bool repeat) {
    if (instance->is_interpolating_between_cycles) {
        animation_cycle_t *next_cycle = &instance->cycles->cycles[instance->next_bound_cycle];

        if (dt + instance->current_animation_time > instance->in_between_interpolation_time) {
            instance->is_interpolating_between_cycles = 0;

            if (repeat) {
                instance->current_animation_time = 0.0f;
            }
        }
        else {
            float current_time = fmod(instance->current_animation_time + dt, instance->in_between_interpolation_time);
            instance->current_animation_time = current_time;

            float progression = instance->current_animation_time / instance->in_between_interpolation_time;

            s_calculate_in_between_bone_space_transforms(
                progression,
                instance,
                next_cycle);

            s_calculate_in_between_final_offsets(
                instance,
                0,
                matrix4_t(1.0f));
        }
    }

    // Is not else so that we can continue directly from interpolating between two cycles
    if (!instance->is_interpolating_between_cycles) {
        animation_cycle_t *bound_cycle = &instance->cycles->cycles[instance->next_bound_cycle];

        if (dt + instance->current_animation_time > bound_cycle->duration) {
            if (repeat) {
                for (uint32_t i = 0; i < instance->skeleton->joint_count; ++i) {
                    instance->current_position_indices[i] = 0;
                    instance->current_rotation_indices[i] = 0;
                    instance->current_scale_indices[i] = 0;
                }
            }
            else {
                dt = 0.0f;
            }
        }

        float current_time = fmod(instance->current_animation_time + dt, bound_cycle->duration);
        instance->current_animation_time = current_time;

        s_calculate_bone_space_transforms(instance, bound_cycle, current_time);
        s_calculate_final_offset(instance, 0, glm::mat4(1.0f));
    }
}

void switch_to_cycle(
    animated_instance_t *instance,
    uint32_t cycle_index,
    bool force) {
    instance->current_animation_time = 0.0f;
    if (!force) {
        instance->is_interpolating_between_cycles = 1;
    }

    instance->prev_bound_cycle = instance->next_bound_cycle;
    instance->next_bound_cycle = cycle_index;

    for (uint32_t i = 0; i < instance->skeleton->joint_count; ++i) {
        instance->current_position_indices[i] = 0;
        instance->current_rotation_indices[i] = 0;
        instance->current_scale_indices[i] = 0;
    }
}    

void sync_gpu_with_animated_transforms(
    animated_instance_t *instance,
    VkCommandBuffer command_buffer) {
    update_gpu_buffer(
        command_buffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(matrix4_t) * instance->skeleton->joint_count,
        instance->interpolated_transforms,
        &instance->interpolated_transforms_ubo);
}

// This is retarted
// TODO: Make sure to not have to set the viewport and bind pipelines for each mesh ASAP
void submit_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data) {
    vkCmdPushConstants(command_buffer, shader->layout, shader->flags, 0, sizeof(mesh_render_data_t), render_data);
    
    if (mesh_has_buffer(BT_INDICES, mesh)) {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(command_buffer, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_mesh_shadow(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t * render_data) {
    VkViewport viewport = {};
    viewport.width = (float)r_shadow_extent().width;
    viewport.height = (float)r_shadow_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_shadow_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    VkDescriptorSet lighting_transforms = r_lighting_uniform();

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        1,
        &lighting_transforms,
        0,
        NULL);

    vkCmdPushConstants(command_buffer, shader->layout, shader->flags, 0, sizeof(mesh_render_data_t), render_data);

    if (mesh_has_buffer(BT_INDICES, mesh)) {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, mesh->vertex_offset, 0);
    }
    else {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(command_buffer, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)r_swapchain_extent().width;
    viewport.height = (float)r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    VkDescriptorSet camera_transforms = r_camera_transforms_uniform();
    VkDescriptorSet descriptor_sets[] = {camera_transforms, instance->descriptor_set};

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(command_buffer, shader->layout, shader->flags, 0, sizeof(mesh_render_data_t), render_data);

    
    if (mesh_has_buffer(BT_INDICES, mesh)) {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(command_buffer, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    void *render_data,
    uint32_t render_data_size,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)r_swapchain_extent().width;
    viewport.height = (float)r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    VkDescriptorSet camera_transforms = r_camera_transforms_uniform();
    VkDescriptorSet descriptor_sets[] = {camera_transforms, instance->descriptor_set};

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(command_buffer, shader->layout, shader->flags, 0, render_data_size, render_data);
    
    if (mesh_has_buffer(BT_INDICES, mesh)) {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(command_buffer, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh_shadow(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t * render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)r_shadow_extent().width;
    viewport.height = (float)r_shadow_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_shadow_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    VkDescriptorSet lighting_transforms = r_lighting_uniform();
    VkDescriptorSet descriptor_sets[] = { lighting_transforms, instance->descriptor_set };

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(command_buffer, shader->layout, shader->flags, 0, sizeof(mesh_render_data_t), render_data);

    if (mesh_has_buffer(BT_INDICES, mesh)) {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, mesh->vertex_offset, 0);
    }
    else {
        vkCmdBindVertexBuffers(command_buffer, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(command_buffer, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void begin_mesh_submission(
    VkCommandBuffer command_buffer,
    shader_t *shader,
    VkDescriptorSet extra_set) {
    VkViewport viewport = {};
    viewport.width = (float)r_swapchain_extent().width;
    viewport.height = (float)r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    VkDescriptorSet sets[] = {r_camera_transforms_uniform(), extra_set};
    uint32_t count = (extra_set == VK_NULL_HANDLE) ? 1 : 2;

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        count,
        sets,
        0,
        NULL);
}
