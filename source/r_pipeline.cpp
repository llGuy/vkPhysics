#include "renderer.hpp"
#include "r_internal.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

static rpipeline_stage_t deferred;

rpipeline_stage_t *r_deferred_stage() {
    return &deferred;
}

static void s_deferred_init() {
    deferred.color_attachment_count = 3;
    deferred.color_attachments = FL_MALLOC(attachment_t, deferred.color_attachment_count + 1);

    VkExtent2D swapchain_extent = r_swapchain_extent();

    VkExtent3D extent = {};
    extent.width = swapchain_extent.width;
    extent.height = swapchain_extent.height;
    extent.depth = 1;

    for (uint32_t i = 0; i < deferred.color_attachment_count; ++i) {
        deferred.color_attachments[i] = r_create_color_attachment(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    deferred.depth_attachment = &deferred.color_attachments[deferred.color_attachment_count];
    *deferred.depth_attachment = r_create_depth_attachment(extent);
    
    VkAttachmentDescription *attachment_descriptions = FL_MALLOC(VkAttachmentDescription, deferred.color_attachment_count + 1);
    for (uint32_t i = 0; i < deferred.color_attachment_count; ++i) {
        attachment_descriptions[i] = r_fill_color_attachment_description(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    attachment_descriptions[deferred.color_attachment_count] = r_fill_depth_attachment_description(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkAttachmentReference *attachment_references = FL_MALLOC(VkAttachmentReference, deferred.color_attachment_count + 1);
    for (uint32_t i = 0; i < deferred.color_attachment_count; ++i) {
        attachment_references[i].attachment = i;
        attachment_references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    attachment_references[deferred.color_attachment_count].attachment = deferred.color_attachment_count;
    attachment_references[deferred.color_attachment_count].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = deferred.color_attachment_count;
    subpass_description.pColorAttachments = attachment_references;
    subpass_description.pDepthStencilAttachment = &attachment_references[deferred.color_attachment_count];

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = deferred.color_attachment_count + 1;
    render_pass_info.pAttachments = attachment_descriptions;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &deferred.render_pass));

    free(attachment_descriptions);
    free(attachment_references);

    deferred.framebuffer = r_create_framebuffer(
        deferred.color_attachment_count,
        deferred.color_attachments,
        deferred.depth_attachment,
        deferred.render_pass,
        swapchain_extent,
        1);

    r_rpipeline_descriptor_set_output_init(&deferred);
}

VkPipelineColorBlendStateCreateInfo r_fill_blend_state_info(rpipeline_stage_t *stage) {
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
        default:
            break;
        }

        attachment_states[attachment].blendEnable = VK_FALSE;
        attachment_states[attachment].srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        attachment_states[attachment].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        attachment_states[attachment].colorBlendOp = VK_BLEND_OP_ADD; // Optional
        attachment_states[attachment].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        attachment_states[attachment].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        attachment_states[attachment].alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    }

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.attachmentCount = stage->color_attachment_count;
    blend_info.pAttachments = attachment_states;
    blend_info.logicOp = VK_LOGIC_OP_COPY;

    return blend_info;
}

void r_free_blend_state_info(VkPipelineColorBlendStateCreateInfo *info) {
    free((void *)info->pAttachments);
}

struct rpipeline_shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

static rpipeline_shader_t s_create_rendering_pipeline_shader(
    const char *vertex_shader_path,
    const char *fragment_shader_path,
    rpipeline_stage_t *stage,
    VkPipelineLayout layout) {
    const char *shaders[] = {vertex_shader_path, fragment_shader_path};
    VkPipelineShaderStageCreateInfo *shader_infos = r_fill_shader_stage_create_infos(shaders, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    /* Is all zero for rendering pipeline shaders */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

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

    VkPipelineColorBlendStateCreateInfo blend_info = r_fill_blend_state_info(stage);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = 1;
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = NULL;
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

    rpipeline_shader_t rendering_pipeline_shader = {};
    rendering_pipeline_shader.pipeline = pipeline;
    rendering_pipeline_shader.layout = layout;

    return rendering_pipeline_shader;
}

static rpipeline_stage_t final_stage;
static rpipeline_shader_t final_shader;

static void s_final_init() {
    final_stage.color_attachment_count = 1;
    final_stage.color_attachments = FL_MALLOC(attachment_t, 1);
    final_stage.color_attachments[0].format = r_swapchain_format();
    final_stage.depth_attachment = NULL;
    final_stage.render_pass = r_final_render_pass();
    
    VkDescriptorSetLayout input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, deferred.binding_count),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    final_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/final.vert.spv",
        "../shaders/SPV/final.frag.spv",
        &final_stage,
        pipeline_layout);
}

void r_execute_final_pass(
    VkCommandBuffer command_buffer) {
    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, final_shader.pipeline);

    VkDescriptorSet inputs[] = {
        deferred.descriptor_set,
        /* Contains lighting information */
        r_lighting_uniform(),
        /* Contains camera information */
        r_camera_transforms_uniform(),
        r_diffuse_ibl_irradiance()
    };
    
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        final_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);
}

void r_pipeline_init() {
    s_deferred_init();
    s_final_init();
}

void begin_scene_rendering(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values[4] = {};
    
    memset(clear_values, 0, sizeof(VkClearValue) * 4);

    clear_values[3].depthStencil.depth = 1.0f;

    VkRect2D render_area = {};
    render_area.extent = r_swapchain_extent();

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = deferred.framebuffer;
    begin_info.renderPass = deferred.render_pass;
    begin_info.clearValueCount = deferred.color_attachment_count + 1;
    begin_info.pClearValues = clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void end_scene_rendering(
    VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}
