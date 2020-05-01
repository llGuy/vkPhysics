#include <math.h>
#include <stdio.h>
#include <string.h>
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include <common/tools.hpp>
#include <common/allocators.hpp>

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
        default: {
            LOG_ERROR("Mesh buffer not supported\n");
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
    VkCullModeFlags culling) {
    VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    return create_3d_shader_color(
        binding_info,
        sizeof(mesh_render_data_t),
        &ubo_type, 1,
        shader_paths,
        shader_flags,
        culling);
}

shader_t create_mesh_shader_shadow(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags) {
    VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    return create_3d_shader_shadow(
        binding_info,
        sizeof(mesh_render_data_t),
        &ubo_type, 1,
        shader_paths,
        shader_flags);
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

void submit_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data) {
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

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        1,
        &camera_transforms,
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
