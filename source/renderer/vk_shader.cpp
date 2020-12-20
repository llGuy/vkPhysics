#include "vk_shader.hpp"
#include "vk_context.hpp"
#include "vk_render_pipeline.hpp"

#include <common/tools.hpp>
#include <common/files.hpp>
#include <common/allocators.hpp>

namespace vk {

static char *s_read_shader(
    const char *path,
    uint32_t *file_size) {
    file_handle_t handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(handle);

    free_file(handle);

    *file_size = contents.size;

    return (char *)contents.data;
}

VkPipelineShaderStageCreateInfo *fill_shader_stage_create_infos(const char **paths, VkShaderStageFlags flags) {
    uint32_t count = pop_count(flags);

    VkShaderStageFlagBits bits_order[] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    VkPipelineShaderStageCreateInfo *infos = flmalloc<VkPipelineShaderStageCreateInfo>(count);
    memset(infos, 0, sizeof(VkPipelineShaderStageCreateInfo) * count);
    VkShaderModule *modules = flmalloc<VkShaderModule>(count);
    
    for (uint32_t bit = 0, current = 0; bit < sizeof(bits_order) / sizeof(VkShaderStageFlagBits); ++bit) {
        if (flags & bits_order[bit]) {
            uint32_t code_length;
            char *code = s_read_shader(paths[current], &code_length);

            VkShaderModuleCreateInfo shader_info = {};
            shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shader_info.codeSize = code_length;
            shader_info.pCode = (uint32_t *)code;
            vkCreateShaderModule(g_ctx->device, &shader_info, NULL, &modules[current]);

            infos[current].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            infos[current].pName = "main";
            infos[current].stage = bits_order[bit];
            infos[current].module = modules[current];

            ++current;
        }
    }

    flfree(modules);

    return infos;
}

VkPipelineColorBlendStateCreateInfo fill_blend_state_info(
    render_pipeline_stage_t *stage,
    alpha_blending_t alpha_blending) {
    VkPipelineColorBlendAttachmentState *attachment_states = FL_MALLOC(VkPipelineColorBlendAttachmentState, stage->color_attachment_count);
    memset(attachment_states, 0, sizeof(VkPipelineColorBlendAttachmentState) * stage->color_attachment_count);
    for (uint32_t attachment = 0; attachment < stage->color_attachment_count; ++attachment) {
        switch (stage->color_attachments[attachment].format) {
        case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R16G16B16A16_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            break;
        }
        case VK_FORMAT_R16G16_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT;
            break;
        }
        case VK_FORMAT_R16_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
            break;
        }
        case VK_FORMAT_R32G32_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        } break;
        default:
            break;
        }

        switch (alpha_blending) {
        case AB_NONE: default: {
            attachment_states[attachment].blendEnable = VK_FALSE;
            attachment_states[attachment].srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            attachment_states[attachment].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            attachment_states[attachment].colorBlendOp = VK_BLEND_OP_ADD; // Optional
            attachment_states[attachment].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            attachment_states[attachment].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            attachment_states[attachment].alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        } break;
        case AB_ONE_MINUS_SRC_ALPHA: {
            attachment_states[attachment].blendEnable = VK_TRUE;
            attachment_states[attachment].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
            attachment_states[attachment].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
            attachment_states[attachment].colorBlendOp = VK_BLEND_OP_ADD; // Optional
            attachment_states[attachment].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            attachment_states[attachment].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            attachment_states[attachment].alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        } break;
        }
    }

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.attachmentCount = stage->color_attachment_count;
    blend_info.pAttachments = attachment_states;
    blend_info.logicOp = VK_LOGIC_OP_COPY;

    return blend_info;
}

VkPipelineLayout create_pipeline_layout(
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
        layouts[i] = get_descriptor_layout(descriptor_types[i], 1);
    }
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = descriptor_layout_count;
    pipeline_layout_info.pSetLayouts = layouts;
    if (push_constant_size) {
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    }
    
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(g_ctx->device, &pipeline_layout_info, NULL, &pipeline_layout);

    return pipeline_layout;
}

void shader_t::init_as_2d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    render_pipeline_stage_t *stage,
    VkPrimitiveTopology topology,
    alpha_blending_t alpha_blending) {
    layout = create_pipeline_layout(
        shader_flags,
        descriptor_layout_types,
        descriptor_layout_count,
        push_constant_size);
    
    VkPipelineShaderStageCreateInfo *shader_infos = fill_shader_stage_create_infos(shader_paths, shader_flags);

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
    input_assembly_info.topology = topology;

    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1.0f;

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    
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

    VkPipelineColorBlendStateCreateInfo blend_info = fill_blend_state_info(stage, alpha_blending);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info {};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_info.depthTestEnable = VK_FALSE;
    depth_stencil_info.depthWriteEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = pop_count(shader_flags);
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

    VK_CHECK(vkCreateGraphicsPipelines(g_ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    flfree(shader_infos);
    flfree(blend_info.pAttachments);

    flags = shader_flags;
}

void shader_t::create_as_3d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    render_pipeline_stage_t *stage,
    VkPrimitiveTopology topology,
    VkCullModeFlags cull_mode) {
    layout = create_pipeline_layout(
        shader_flags,
        descriptor_layout_types,
        descriptor_layout_count,
        push_constant_size);
    
    VkPipelineShaderStageCreateInfo *shader_infos = fill_shader_stage_create_infos(shader_paths, shader_flags);

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
    input_assembly_info.topology = topology;

    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1.0f;

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    
    VkPipelineViewportStateCreateInfo viewport_info {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &rect;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = cull_mode;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.minSampleShading = 1.0f;

    VkPipelineColorBlendStateCreateInfo blend_info = fill_blend_state_info(stage);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
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
    pipeline_info.stageCount = pop_count(shader_flags);
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

    VK_CHECK(vkCreateGraphicsPipelines(g_ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    flfree(shader_infos);
    flfree(blend_info.pAttachments);

    flags = shader_flags;
}

shader_t create_as_3d_shader_for_stage(
    stage_type_t stage_type,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    VkCullModeFlags culling) {
    switch (stage_type) {
    case ST_SHADOW: {
        return s_create_3d_shader(
            binding_info,
            push_constant_size,
            descriptor_layout_types,
            descriptor_layout_count,
            shader_paths,
            shader_flags,
            r_shadow_stage(),
            topology,
            VK_CULL_MODE_FRONT_BIT);
    } break;

    case ST_DEFERRED: {
        return s_create_3d_shader(
            binding_info,
            push_constant_size,
            descriptor_layout_types,
            descriptor_layout_count,
            shader_paths,
            shader_flags,
            r_deferred_stage(),
            topology,
            culling);
    } break;

    default: {
    } break;
    }
}

}
