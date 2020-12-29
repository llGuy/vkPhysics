#include "vk.hpp"
#include "vk_buffer.hpp"
#include "vk_scene3d.hpp"
#include "vk_context.hpp"
#include "vk_load_mesh.hpp"
#include "vk_animation.hpp"
#include "vk_stage_shadow.hpp"
#include "vk_render_pipeline.hpp"

#include <log.hpp>
#include <vulkan/vulkan.h>
#include <math.hpp>
#include <files.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

namespace vk {

void mesh_t::push_buffer(buffer_type_t buffer_type) {
    buffer_type_stack[buffer_count++] = buffer_type;
    buffers[buffer_type].type = buffer_type;
}

bool mesh_t::has_buffer(buffer_type_t buffer_type) {
    return buffers[buffer_type].type == buffer_type;
}

mesh_buffer_t *mesh_t::get_mesh_buffer(buffer_type_t buffer_type) {
    if (has_buffer(buffer_type)) {
        return &buffers[buffer_type];
    }
    else {
        return NULL;
    }
}

shader_binding_info_t mesh_t::create_shader_binding_info() {
    shader_binding_info_t info = {};
    uint32_t start_index = 0;
    if (has_buffer(BT_INDICES)) {
        start_index = 1;
    }

    uint32_t binding_count = buffer_count - start_index;

    info.binding_count = binding_count;
    info.binding_descriptions = FL_MALLOC(VkVertexInputBindingDescription, info.binding_count);

    // Each binding will store one attribute
    info.attribute_count = binding_count;
    info.attribute_descriptions = FL_MALLOC(VkVertexInputAttributeDescription, info.binding_count);

    for (uint32_t i = 0; start_index < buffer_count; ++start_index, ++i) {
        VkVertexInputBindingDescription *current_binding = &info.binding_descriptions[i];
        VkVertexInputAttributeDescription *current_attribute = &info.attribute_descriptions[i];

        // These depend on the buffer type
        VkFormat format;
        uint32_t stride;

        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(buffer_type_stack[start_index]);
        
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

void mesh_t::init_mesh_vbo_final_list() {
    uint32_t vb_count = has_buffer(BT_INDICES) ? buffer_count - 1 : buffer_count;
    vertex_buffer_count = vb_count;

    uint32_t counter = 0;
    for (uint32_t i = 0; i < buffer_count; ++i) {
        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(buffer_type_stack[i]);
        
        if (current_mesh_buffer->type == BT_INDICES) {
            index_buffer = current_mesh_buffer->gpu_buffer.buffer;
        }
        else {
            vertex_buffers_final[counter++] = current_mesh_buffer->gpu_buffer.buffer;
        }
    }

    for (uint32_t i = 0; i < vb_count; ++i) {
        vertex_buffers_offsets[i] = vertex_offset;
    }
}

void mesh_t::load_sphere(shader_binding_info_t *binding_info) {
    int32_t sector_count = 16;
    int32_t stack_count = 16;
    
    uint32_t vertex_count = (sector_count + 1) * (stack_count + 1);
    uint32_t index_count = sector_count * stack_count * 6;
    
    vector3_t *positions = FL_MALLOC(vector3_t, vertex_count);

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

    push_buffer(BT_INDICES);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES);
    indices_gpu_buffer->gpu_buffer.init(
        sizeof(uint32_t) * index_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    push_buffer(BT_VERTEX);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX);
    vertex_gpu_buffer->gpu_buffer.init(
        sizeof(vector3_t) * vertex_count,
        positions,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (binding_info) {
        *binding_info = create_shader_binding_info();
    }

    vertex_offset = 0;
    vertex_count = vertex_count;
    first_index = 0;
    index_offset = 0;
    index_count = index_count;
    index_type = VK_INDEX_TYPE_UINT32;

    init_mesh_vbo_final_list();

    FL_FREE(positions);
    
    FL_FREE(indices);
}

void mesh_t::load_cube(shader_binding_info_t *binding_info) {
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

    push_buffer(BT_INDICES);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES);
    indices_gpu_buffer->gpu_buffer.init(
        sizeof(uint32_t) * index_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    push_buffer(BT_VERTEX);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX);
    vertex_gpu_buffer->gpu_buffer.init(
        sizeof(vector3_t) * vertex_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (binding_info) {
        *binding_info = create_shader_binding_info();
    }

    vertex_offset = 0;
    vertex_count = vertex_count;
    first_index = 0;
    index_offset = 0;
    index_count = index_count;
    index_type = VK_INDEX_TYPE_UINT32;

    init_mesh_vbo_final_list();
}

void mesh_t::load_external(shader_binding_info_t *binding_info, const char *path) {
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

    vector3_t *vertices = get_vertices(file_header.vertices_count, &serialiser);
    vector3_t *normals = get_normals(file_header.normals_count, &serialiser);
    vector2_t *tcoords = get_tcoords(file_header.tcoords_count, &serialiser);
    vector4_t *joint_weights = get_joint_weights(file_header.joint_weights_count, &serialiser);
    ivector4_t *joint_ids = get_joint_ids(file_header.joint_ids_count, &serialiser);
    uint32_t *indices = get_indices(file_header.indices_count, &serialiser);

    push_buffer(BT_INDICES);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES);
    indices_gpu_buffer->gpu_buffer.init(
        sizeof(uint32_t) * file_header.indices_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer(BT_VERTEX);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX);
    vertex_gpu_buffer->gpu_buffer.init(
        sizeof(vector3_t) * file_header.vertices_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer(BT_NORMAL);
    mesh_buffer_t *normal_gpu_buffer = get_mesh_buffer(BT_NORMAL);
    normal_gpu_buffer->gpu_buffer.init(
        sizeof(vector3_t) * file_header.normals_count,
        normals,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (file_header.joint_weights_count) {
        push_buffer(BT_JOINT_WEIGHT);
        mesh_buffer_t *weights_gpu_buffer = get_mesh_buffer(BT_JOINT_WEIGHT);
        weights_gpu_buffer->gpu_buffer.init(
            sizeof(vector4_t) * file_header.joint_weights_count,
            joint_weights,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    
    if (file_header.joint_ids_count) {
        push_buffer(BT_JOINT_INDICES);
        mesh_buffer_t *joints_gpu_buffer = get_mesh_buffer(BT_JOINT_INDICES);
        joints_gpu_buffer->gpu_buffer.init(
            sizeof(ivector4_t) * file_header.joint_ids_count,
            joint_ids,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    if (binding_info) {
        *binding_info = create_shader_binding_info();
    }

    vertex_offset = 0;
    vertex_count = file_header.vertices_count;
    first_index = 0;
    index_offset = 0;
    index_count = file_header.indices_count;
    index_type = VK_INDEX_TYPE_UINT32;

    init_mesh_vbo_final_list();
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

    get_vertices(file_header.vertices_count, &serialiser, vertices);
    get_normals(file_header.normals_count, &serialiser, normals);
    get_tcoords(file_header.tcoords_count, &serialiser, tcoords);
    get_joint_weights(file_header.joint_weights_count, &serialiser, joint_weights);
    get_joint_ids(file_header.joint_ids_count, &serialiser, joint_ids);
    dude_indices = get_indices(file_header.indices_count, &serialiser);

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
    dst_merged->push_buffer(BT_INDICES);
    mesh_buffer_t *merged_indices_buffer = dst_merged->get_mesh_buffer(BT_INDICES);
    merged_indices_buffer->gpu_buffer.init(
        sizeof(uint32_t) * total_indices_count,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    dst_merged->push_buffer(BT_VERTEX);
    mesh_buffer_t *merged_vertex_buffer = dst_merged->get_mesh_buffer(BT_VERTEX);
    merged_vertex_buffer->gpu_buffer.init(
        sizeof(vector3_t) * (file_header.vertices_count + sphere_vertex_count),
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    dst_merged->push_buffer(BT_NORMAL);
    mesh_buffer_t *merged_normal_buffer = dst_merged->get_mesh_buffer(BT_NORMAL);
    merged_normal_buffer->gpu_buffer.init(
        sizeof(vector3_t) * (file_header.normals_count + sphere_vertex_count),
        normals,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    dst_merged->push_buffer(BT_JOINT_WEIGHT);
    mesh_buffer_t *weights_gpu_buffer = dst_merged->get_mesh_buffer(BT_JOINT_WEIGHT);
    weights_gpu_buffer->gpu_buffer.init(
        sizeof(vector4_t) * file_header.joint_weights_count + sphere_vertex_count,
        joint_weights,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
    dst_merged->push_buffer(BT_JOINT_INDICES);
    mesh_buffer_t *joints_gpu_buffer = dst_merged->get_mesh_buffer(BT_JOINT_INDICES);
    joints_gpu_buffer->gpu_buffer.init(
        sizeof(ivector4_t) * file_header.joint_ids_count + sphere_vertex_count,
        joint_ids,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    dst_merged->vertex_offset = 0;
    dst_merged->vertex_count = file_header.vertices_count + sphere_vertex_count;
    dst_merged->first_index = 0;
    dst_merged->index_offset = 0;
    dst_merged->index_count = total_indices_count;
    dst_merged->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_merged = dst_merged->create_shader_binding_info();
    dst_merged->init_mesh_vbo_final_list();

    // Create player mesh
    dst_a->push_buffer(BT_INDICES);
    mesh_buffer_t *a_indices_buffer = dst_a->get_mesh_buffer(BT_INDICES);
    a_indices_buffer->gpu_buffer.init(
        sizeof(uint32_t) * file_header.indices_count,
        dude_indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    dst_a->push_buffer(BT_VERTEX);
    mesh_buffer_t *a_vertex_buffer = dst_a->get_mesh_buffer(BT_VERTEX);
    a_vertex_buffer->gpu_buffer = merged_vertex_buffer->gpu_buffer;

    dst_a->push_buffer(BT_NORMAL);
    mesh_buffer_t *a_normal_buffer = dst_a->get_mesh_buffer(BT_NORMAL);
    a_normal_buffer->gpu_buffer = merged_normal_buffer->gpu_buffer;

    dst_a->push_buffer(BT_JOINT_WEIGHT);
    mesh_buffer_t *a_joint_weight_buffer = dst_a->get_mesh_buffer(BT_JOINT_WEIGHT);
    a_joint_weight_buffer->gpu_buffer = weights_gpu_buffer->gpu_buffer;

    dst_a->push_buffer(BT_JOINT_INDICES);
    mesh_buffer_t *a_joint_indices_buffer = dst_a->get_mesh_buffer(BT_JOINT_INDICES);
    a_joint_indices_buffer->gpu_buffer = joints_gpu_buffer->gpu_buffer;

    dst_a->vertex_offset = 0;
    dst_a->vertex_count = file_header.vertices_count;
    dst_a->first_index = 0;
    dst_a->index_offset = 0;
    dst_a->index_count = file_header.indices_count;
    dst_a->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_a = dst_a->create_shader_binding_info();
    dst_a->init_mesh_vbo_final_list();

    // Create ball mesh
    dst_b->push_buffer(BT_INDICES);
    mesh_buffer_t *b_indices_buffer = dst_b->get_mesh_buffer(BT_INDICES);
    b_indices_buffer->gpu_buffer.init(
        sizeof(uint32_t) * sphere_index_count,
        sphere_indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    dst_b->push_buffer(BT_VERTEX);
    mesh_buffer_t *b_vertex_buffer = dst_b->get_mesh_buffer(BT_VERTEX);
    b_vertex_buffer->gpu_buffer = merged_vertex_buffer->gpu_buffer;

    dst_b->vertex_offset = sizeof(vector3_t) * file_header.vertices_count;
    dst_b->vertex_count = sphere_vertex_count;
    dst_b->first_index = 0;
    dst_b->index_offset = 0;
    dst_b->index_count = sphere_index_count;
    dst_b->index_type = VK_INDEX_TYPE_UINT32;

    *sbi_b = dst_b->create_shader_binding_info();
    dst_b->init_mesh_vbo_final_list();
}

shader_t create_mesh_shader_color(shader_create_info_t *info, mesh_type_flags_t mesh_type) {
    VkDescriptorType types[] = {
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

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

    info->descriptor_layout_types = types;
    info->descriptor_layout_count = type_count;
    info->push_constant_size = push_constant_size;

    shader_t s = {};
    s.init_as_3d_shader_for_stage(info, ST_DEFERRED);

    return s;
}

// shader_t create_mesh_shader_shadow(
//     shader_binding_info_t *binding_info,
//     const char **shader_paths,
//     VkShaderStageFlags shader_flags,
//     VkPrimitiveTopology topology,
//     mesh_type_flags_t mesh_type) {
shader_t create_mesh_shader_shadow(shader_create_info_t *info, mesh_type_flags_t mesh_type) {
    if (mesh_type == MT_STATIC) {
        VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        uint32_t push_constant_size = sizeof(mesh_render_data_t);
        if (mesh_type & MT_MERGED_MESH) {
            push_constant_size += sizeof(matrix4_t) + sizeof(float);
        }

        info->push_constant_size = push_constant_size;
        info->descriptor_layout_count = 1;
        info->descriptor_layout_types = &ubo_type;
        info->cull_mode = VK_CULL_MODE_FRONT_BIT;

        shader_t s = {}; s.init_as_3d_shader_for_stage(info, ST_SHADOW);
    
        return s;
    }
    else {
        VkDescriptorType ubo_types[] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};

        uint32_t push_constant_size = sizeof(mesh_render_data_t);
        if (mesh_type & MT_MERGED_MESH) {
            push_constant_size += sizeof(matrix4_t) + sizeof(float);
        }

        info->push_constant_size = push_constant_size;
        info->descriptor_layout_count = 2;
        info->descriptor_layout_types = ubo_types;
        info->cull_mode = VK_CULL_MODE_FRONT_BIT;

        shader_t s = {};
        s.init_as_3d_shader_for_stage(info, ST_SHADOW);

        return s;
    }
}

void submit_mesh(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data) {
    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, render_data.size, render_data.p);
    
    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_mesh_shadow(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->pipeline.shadow->map_extent.width;
    viewport.height = (float)g_ctx->pipeline.shadow->map_extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->pipeline.shadow->map_extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();
    VkDescriptorSet lighting_transforms = scene_sets.lighting_ubo;

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        1,
        &lighting_transforms,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, render_data.size, render_data.p);

    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet camera_transforms = scene_sets.camera_ubo;
    VkDescriptorSet descriptor_sets[] = {camera_transforms, instance->descriptor_set};

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, render_data.size, render_data.p);
    
    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh_shadow(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->pipeline.shadow->map_extent.width;
    viewport.height = (float)g_ctx->pipeline.shadow->map_extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->pipeline.shadow->map_extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet lighting_transforms = scene_sets.lighting_ubo;
    VkDescriptorSet descriptor_sets[] = { lighting_transforms, instance->descriptor_set };

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, render_data.size, render_data.p);

    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, mesh->vertex_offset, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void begin_mesh_submission(
    VkCommandBuffer cmdbuf,
    shader_t *shader,
    VkDescriptorSet extra_set) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet sets[] = {scene_sets.camera_ubo, extra_set};
    uint32_t count = (extra_set == VK_NULL_HANDLE) ? 1 : 2;

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        count,
        sets,
        0,
        NULL);
}

}
