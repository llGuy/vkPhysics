#include <math.h>
#include <stdio.h>
#include <string.h>
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include <common/tools.hpp>
#include <common/files.hpp>
#include <common/string.hpp>
#include <common/allocators.hpp>
#include <common/serialiser.hpp>
#include <common/containers.hpp>

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
            format = VK_FORMAT_R32G32B32A32_UINT;
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
    mesh_type_flags_t mesh_type) {
    VkDescriptorType types[] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    uint32_t type_count = 1;
    if (mesh_type & MT_ANIMATED) {
        type_count++;
    }

    if (mesh_type & MT_PASS_EXTRA_UNIFORM_BUFFER) {
        type_count++;
    }

    return create_3d_shader_color(
        binding_info,
        sizeof(mesh_render_data_t),
        types, type_count,
        shader_paths,
        shader_flags,
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

    memset(mesh->vertex_buffers_offsets, 0, sizeof(VkDeviceSize) * vb_count);
}

static void s_load_sphere(
    mesh_t *mesh,
    shader_binding_info_t *binding_info) {
    const float PI = 3.14159265359f;

    int32_t sector_count = 32;
    int32_t stack_count = 32;
    
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

    vector3_t *vertices = LN_MALLOC(vector3_t, file_header.vertices_count);
    vector3_t *normals = LN_MALLOC(vector3_t, file_header.normals_count);
    vector2_t *tcoords = LN_MALLOC(vector2_t, file_header.tcoords_count);
    vector4_t *joint_weights = LN_MALLOC(vector4_t, file_header.joint_weights_count);
    ivector4_t *joint_ids = LN_MALLOC(ivector4_t, file_header.joint_ids_count);
    uint32_t *indices = LN_MALLOC(uint32_t, file_header.indices_count);

    for (uint32_t i = 0; i < file_header.vertices_count; ++i) {
        vertices[i] = serialiser.deserialise_vector3();
    }

    for (uint32_t i = 0; i < file_header.normals_count; ++i) {
        normals[i] = serialiser.deserialise_vector3();
    }

    for (uint32_t i = 0; i < file_header.tcoords_count; ++i) {
        tcoords[i].x = serialiser.deserialise_float32();
        tcoords[i].y = serialiser.deserialise_float32();
    }

    for (uint32_t i = 0; i < file_header.joint_weights_count; ++i) {
        joint_weights[i].x = serialiser.deserialise_float32();
        joint_weights[i].y = serialiser.deserialise_float32();
        joint_weights[i].z = serialiser.deserialise_float32();
        joint_weights[i].w = serialiser.deserialise_float32();
        float sum = joint_weights[i].x + joint_weights[i].y + joint_weights[i].z + joint_weights[i].w;
        float q = 1.0f / sum;
        joint_weights[i] *= q;
    }

    for (uint32_t i = 0; i < file_header.joint_ids_count; ++i) {
        joint_ids[i].x = (int32_t)serialiser.deserialise_uint32();
        joint_ids[i].y = (int32_t)serialiser.deserialise_uint32();
        joint_ids[i].z = (int32_t)serialiser.deserialise_uint32();
        joint_ids[i].w = (int32_t)serialiser.deserialise_uint32();
    }

    for (uint32_t i = 0; i < file_header.indices_count; ++i) {
        indices[i] = serialiser.deserialise_uint32();
    }

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

static bool s_str_comp(
    const char *a,
    uint32_t a_len,
    const char *b,
    uint32_t b_len) {
    if (a_len == b_len) {
        for (uint32_t i = 0; i < a_len; ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }

        return true;
    }

    return false;
}

static void s_fill_animation_linker(
    const char *linker_path) {
    file_handle_t file_handle = create_file(linker_path, FLF_NONE);
    file_contents_t contents = read_file(file_handle); // Just text
    free_file(file_handle);

    enum parser_state_t {
        PS_NONE,
        PS_COMMENT,
        PS_NEW_INSTRUCTION,
        PS_END_OF_LINE,
        PS_INSTRUCTION_KEYWORD,
        PS_WHITESPACE,
        PS_A_KEYWORD,
        PS_LINK_INSTRUCTION
    } parser_state;

    const char *current_token = NULL;
    uint32_t current_token_length = 0;

    const char *link_parameter1 = NULL;
    uint32_t link_parameter1_length = 0;

    const char *link_parameter2 = NULL;
    uint32_t link_parameter2_length = 0;

    for (char *c = (char *)contents.data; c != 0; ++c) {
        // Get parser state
        if (*c == '#') {
            // Go until end of line
            parser_state = PS_COMMENT;
        }
        else if (*c == '\n') {
            parser_state = PS_END_OF_LINE;
        }

        if (parser_state != PS_COMMENT) {
            if (*c == ' ' || *c == '\t') {
                parser_state = PS_WHITESPACE;
            }
            else if (parser_state == PS_WHITESPACE) {
                parser_state = PS_A_KEYWORD;
            }
        }

        switch (parser_state) {
        case PS_COMMENT: {
        } break;

        case PS_END_OF_LINE: {
            parser_state = PS_NEW_INSTRUCTION;
        } break;

        case PS_NEW_INSTRUCTION: {
            current_token_length += 1;
            parser_state = PS_WHITESPACE;
        } break;

        case PS_WHITESPACE: {
            if (current_token) {
                // Process keyword
                if (s_str_comp(current_token, current_token_length, "link", strlen("link"))) {
                    parser_state = PS_LINK_INSTRUCTION;
                }
            }
            else {
                // Do nothing
            }
        }
        }
    }
}

void load_animation_cycles(
    animation_cycles_t *cycles,
    const char *linker_path,
    const char *path) {
    // s_fill_animation_linker(linker_path);

    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    cycles->cycle_count = serialiser.deserialise_uint32();
    cycles->cycles = FL_MALLOC(animation_cycle_t, cycles->cycle_count);

    for (uint32_t i = 0; i < cycles->cycle_count; ++i) {
        animation_cycle_t *cycle = &cycles->cycles[i];
        cycle->animation_name = create_fl_string(serialiser.deserialise_string());
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
}

void animated_instance_init(
    animated_instance_t *instance,
    skeleton_t *skeleton,
    animation_cycles_t *cycles) {
    memset(instance, 0, sizeof(animated_instance_t));
    instance->current_animation_time = 0.0f;
    instance->is_interpolating_between_cycles = 0;
    instance->skeleton = skeleton;
    instance->next_bound_cycle = 2;
    instance->interpolated_transforms = FL_MALLOC(matrix4_t, skeleton->joint_count);
    instance->current_positions = FL_MALLOC(vector3_t, skeleton->joint_count);
    instance->current_rotations = FL_MALLOC(quaternion_t, skeleton->joint_count);
    instance->current_scales = FL_MALLOC(vector3_t, skeleton->joint_count);
    instance->current_position_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->current_rotation_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->current_scale_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    instance->cycles = cycles;

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
    float dt) {
    animation_cycle_t *bound_cycle = &instance->cycles->cycles[instance->next_bound_cycle];

    if (dt + instance->current_animation_time > bound_cycle->duration) {
        for (uint32_t i = 0; i < instance->skeleton->joint_count; ++i) {
            instance->current_position_indices[i] = 0;
            instance->current_rotation_indices[i] = 0;
            instance->current_scale_indices[i] = 0;
        }
    }

    float current_time = fmod(instance->current_animation_time + dt, bound_cycle->duration);
    instance->current_animation_time = current_time;

    s_calculate_bone_space_transforms(instance, bound_cycle, current_time);
    s_calculate_final_offset(instance, 0, glm::mat4(1.0f));
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

void submit_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data,
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

        vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, mesh->first_index, mesh->vertex_offset, 0);
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
