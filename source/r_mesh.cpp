#include <math.h>
#include <stdio.h>
#include <string.h>
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

// For the moment, just puts the camera info uniform buffer
static VkPipelineLayout s_create_mesh_shader_layout(
    VkShaderStageFlags shader_flags,
    VkDescriptorType *descriptor_types,
    uint32_t descriptor_layout_count,
    uint32_t push_constant_size) {
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = shader_flags;
    push_constant_range.offset = 0;
    push_constant_range.size = push_constant_size;

    VkDescriptorSetLayout *layouts = ALLOCA(VkDescriptorSetLayout, descriptor_layout_count);
    for (uint32_t i = 0; i < descriptor_layout_count; ++i) {
        layouts[i] = r_descriptor_layout(descriptor_types[i], 1);
    }
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = descriptor_layout_count;
    pipeline_layout_info.pSetLayouts = layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);

    return pipeline_layout;
}

mesh_shader_t create_mesh_shader(
    mesh_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags) {
    VkPipelineLayout layout = s_create_mesh_shader_layout(
        shader_flags,
        descriptor_layout_types,
        descriptor_layout_count,
        push_constant_size);
    
    VkPipelineShaderStageCreateInfo *shader_infos = r_fill_shader_stage_create_infos(shader_paths, shader_flags);

    /* Is all zero for rendering pipeline shaders */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    if (binding_info) {
        vertex_input_info.vertexBindingDescriptionCount = binding_info->binding_count;
        vertex_input_info.pVertexBindingDescriptions = binding_info->binding_descriptions;
        vertex_input_info.vertexAttributeDescriptionCount = binding_info->attribute_count;
        vertex_input_info.pVertexAttributeDescriptions = binding_info->attribute_descriptions;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1.0f;

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    
    VkPipelineViewportStateCreateInfo viewport_info {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &rect;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.minSampleShading = 1.0f;

    rpipeline_stage_t *stage = r_deferred_stage();
    VkPipelineColorBlendStateCreateInfo blend_info = r_fill_blend_state_info(stage);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = 1;
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info {};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_info.depthTestEnable = VK_TRUE;
    depth_stencil_info.depthWriteEnable = VK_TRUE;
    depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_info.minDepthBounds = 0.0f;
    depth_stencil_info.maxDepthBounds = 1.0f;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = &depth_stencil_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = stage->render_pass;
    /* For now just support one subpass per render pass */
    pipeline_info.subpass = 0;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(r_device(), VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    r_free_shader_stage_create_info(shader_infos);
    r_free_blend_state_info(&blend_info);

    mesh_shader_t mesh_shader = {};
    mesh_shader.pipeline = pipeline;
    mesh_shader.layout = layout;
    mesh_shader.flags = shader_flags;

    return mesh_shader;
}

// May need geometry shader for normal calculations
mesh_shader_t create_mesh_shader(
    mesh_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags) {
    VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    return create_mesh_shader(
        binding_info,
        sizeof(mesh_render_data_t),
        &ubo_type, 1,
        shader_paths,
        shader_flags);
}

static void s_create_mesh_vbo_final_list(mesh_t *mesh) {
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

#define MCHECK(index, max) if (index >= max) { printf("%d, %d", index, max); assert(0); }

static void s_load_sphere(
    mesh_t *mesh,
    mesh_binding_info_t *binding_info) {
    const float PI = 3.14159265359f;

    int32_t sector_count = 64;
    int32_t stack_count = 64;
    
    uint32_t vertex_count = (sector_count + 1) * (stack_count + 1);
    uint32_t index_count = sector_count * stack_count * 6;
    
    vector3_t *positions = FL_MALLOC(vector3_t, vertex_count);
    vector3_t *normals = FL_MALLOC(vector3_t, vertex_count);
    vector2_t *uvs = FL_MALLOC(vector2_t, vertex_count);
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

            positions[counter] = vector3_t(x, z, y);
            uvs[counter++] = vector2_t((float)j / (float)sector_count, (float)i / (float)stack_count);
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

    push_buffer_to_mesh(BT_UVS, mesh);
    mesh_buffer_t *uvs_gpu_buffer = get_mesh_buffer(BT_UVS, mesh);
    uvs_gpu_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector2_t) * vertex_count,
        uvs,
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

    s_create_mesh_vbo_final_list(mesh);

    free(positions);
    free(normals);
    free(uvs);
    free(indices);
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

void submit_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    mesh_shader_t *shader,
    mesh_render_data_t *render_data) {
    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    
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
