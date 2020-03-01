#include <math.h>
#include <stdio.h>
#include "tools.hpp"
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>

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

mesh_binding_info_t create_mesh_binding_info(
    mesh_t *mesh) {
    mesh_binding_info_t info = {};
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
        default: {
            printf("Mesh buffer not supported\n");
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

static void s_load_sphere(
    mesh_t *mesh,
    mesh_binding_info_t *binding_info) {
    const float PI = 3.14159265359f;
    
    uint32_t segment_count_x = 64;
    uint32_t segment_count_y = 64;

    vector3_t *positions = FL_MALLOC(vector3_t, segment_count_x * segment_count_y);
    vector3_t *normals = FL_MALLOC(vector3_t, segment_count_x * segment_count_y);
    vector2_t *uvs = FL_MALLOC(vector2_t, segment_count_x * segment_count_y);
    uint32_t *indices = FL_MALLOC(uint32_t, segment_count_x * segment_count_y * 2);

    uint32_t counter = 0;
    for (uint32_t y = 0; y <= segment_count_y; ++y) {
        for (uint32_t x = 0; x < segment_count_x; ++x) {
            float segment_x = (float)x / (float)segment_count_x;
            float segment_y = (float)y / (float)segment_count_y;
            float x_position = cos(segment_x * 2.0f * PI) * sin(segment_y * PI);
            float y_position = cos(segment_y);
            float z_position = sin(segment_x * 2.0f * PI) * cos(segment_y * PI);

            positions[counter] = vector3_t(x_position, y_position, z_position);
            normals[counter] = vector3_t(x_position, y_position, z_position);
            uvs[counter] = vector2_t(segment_x, segment_y);

            ++counter;
        }
    }

    bool odd = 0;
    counter = 0;

    for (uint32_t y = 0; y < segment_count_y; ++y) {
        if (!odd) {
            for (int32_t x = 0; x <= (int32_t)segment_count_x; ++x) {
                indices[counter++] = y * (segment_count_x + 1) + x;
                indices[counter++] = (y + 1) * (segment_count_x + 1) + x;
            }
        }
        else {
            for (int32_t x = segment_count_x; x >= 0; --x) {
                indices[counter++] = (y + 1) * (segment_count_x + 1) + x;
                indices[counter++] = y * (segment_count_x + 1) + x;
            }
        }

        odd ^= 1;
    }

    push_buffer_to_mesh(BT_INDICES, mesh);
    mesh_buffer_t *indices_gpu_buffer = get_mesh_buffer(BT_INDICES, mesh);
    indices_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(uint32_t) * segment_count_x * segment_count_y,
        indices,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_VERTEX, mesh);
    mesh_buffer_t *vertex_gpu_buffer = get_mesh_buffer(BT_VERTEX, mesh);
    vertex_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * segment_count_x * segment_count_y,
        positions,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_NORMAL, mesh);
    mesh_buffer_t *normal_gpu_buffer = get_mesh_buffer(BT_NORMAL, mesh);
    normal_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * segment_count_x * segment_count_y,
        normals,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    push_buffer_to_mesh(BT_UVS, mesh);
    mesh_buffer_t *uvs_gpu_buffer = get_mesh_buffer(BT_UVS, mesh);
    uvs_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector2_t) * segment_count_x * segment_count_y,
        uvs,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (binding_info) {
        *binding_info = create_mesh_binding_info(mesh);
    }
}

// TODO:
static void s_load_cube(
    mesh_t *mesh) {
    (void)mesh;
}

void load_mesh_internal(
    internal_mesh_type_t mesh_type,
    mesh_t *mesh,
    mesh_binding_info_t *info) {
    memset(mesh, 0, sizeof(mesh_t));

    switch (mesh_type) {
    case IM_SPHERE: {
        s_load_sphere(mesh, info);
    } break;
       
    case IM_CUBE: {
        s_load_cube(mesh);
    } break;
    }
}
