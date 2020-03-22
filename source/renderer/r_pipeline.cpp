#include "renderer.hpp"
#include "r_internal.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

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
        case VK_FORMAT_R16G16_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT;
            break;
        }
        case VK_FORMAT_R16_SFLOAT: {
            attachment_states[attachment].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
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

void destroy_rpipeline_stage(
    rpipeline_stage_t *stage) {
    // No need to destroy the render pass
    vkDestroyFramebuffer(r_device(), stage->framebuffer, NULL);
    for (uint32_t i = 0; i < stage->color_attachment_count; ++i) {
        vkDestroyImageView(r_device(), stage->color_attachments[i].image_view, NULL);
        vkDestroySampler(r_device(), stage->color_attachments[i].sampler, NULL);
        vkFreeMemory(r_device(), stage->color_attachments[i].image_memory, NULL);
        vkDestroyImage(r_device(), stage->color_attachments[i].image, NULL);
    }

    if (stage->depth_attachment) {
        vkDestroyImageView(r_device(), stage->depth_attachment->image_view, NULL);
        vkDestroySampler(r_device(), stage->depth_attachment->sampler, NULL);
        vkFreeMemory(r_device(), stage->depth_attachment->image_memory, NULL);
        vkDestroyImage(r_device(), stage->depth_attachment->image, NULL);
        //FL_FREE(stage->depth_attachment);
    }

    if (stage->descriptor_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(r_device(), r_descriptor_pool(), 1, &stage->descriptor_set);
    }

    if (stage->color_attachments) {
        //FL_FREE(stage->color_attachments);
    }
}

static VkExtent2D shadow_map_extent;
static rpipeline_stage_t shadow_stage;

rpipeline_stage_t *r_shadow_stage() {
    return &shadow_stage;
}

VkExtent2D r_shadow_extent() {
    return shadow_map_extent;
}

static void s_shadow_init() {
    shadow_map_extent.width = 4000;
    shadow_map_extent.height = 4000;

    VkExtent3D extent3d = {};
    extent3d.width = shadow_map_extent.width;
    extent3d.height = shadow_map_extent.height;
    extent3d.depth = 1;
    
    shadow_stage.color_attachment_count = 0;
    shadow_stage.color_attachments = NULL;
    shadow_stage.depth_attachment = FL_MALLOC(attachment_t, 1);
    *shadow_stage.depth_attachment = r_create_depth_attachment(extent3d);

    VkAttachmentDescription attachment_description = r_fill_depth_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 0;
    subpass_description.pColorAttachments = NULL;
    subpass_description.pDepthStencilAttachment = &attachment_reference;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &shadow_stage.render_pass));

    shadow_stage.framebuffer = r_create_framebuffer(
        shadow_stage.color_attachment_count,
        shadow_stage.color_attachments,
        shadow_stage.depth_attachment,
        shadow_stage.render_pass,
        shadow_map_extent,
        1);

    r_rpipeline_descriptor_set_output_init(&shadow_stage);
}

void begin_shadow_rendering(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_value = {};
    clear_value.depthStencil.depth = 1.0f;

    VkRect2D render_area = {};
    render_area.extent = shadow_map_extent;

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = shadow_stage.framebuffer;
    begin_info.renderPass = shadow_stage.render_pass;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void end_shadow_rendering(
    VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}

static rpipeline_stage_t deferred;

rpipeline_stage_t *r_deferred_stage() {
    return &deferred;
}

static void s_deferred_render_pass_init() {
    deferred.color_attachment_count = 4;

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
}

static void s_deferred_init() {
    deferred.color_attachments = FL_MALLOC(attachment_t, deferred.color_attachment_count);
    deferred.depth_attachment = FL_MALLOC(attachment_t, 1);

    VkExtent2D swapchain_extent = r_swapchain_extent();

    VkExtent3D extent = {};
    extent.width = swapchain_extent.width;
    extent.height = swapchain_extent.height;
    extent.depth = 1;

    for (uint32_t i = 0; i < deferred.color_attachment_count; ++i) {
        deferred.color_attachments[i] = r_create_color_attachment(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    *deferred.depth_attachment = r_create_depth_attachment(extent);
    
    deferred.framebuffer = r_create_framebuffer(
        deferred.color_attachment_count,
        deferred.color_attachments,
        deferred.depth_attachment,
        deferred.render_pass,
        swapchain_extent,
        1);

    r_rpipeline_descriptor_set_output_init(&deferred);
}

static void s_destroy_deferred() {
    
}

#define KERNEL_COUNT 54
static struct {
    vector4_t k[KERNEL_COUNT];
    float resolution_coefficient;
} kernels;

static vector4_t noise[16];
static texture_t noise_texture;
static rpipeline_stage_t ssao_stage;
static rpipeline_shader_t ssao_shader;
static gpu_buffer_t kernel_uniform_buffer;
static VkDescriptorSet kernel_uniform_buffer_set;

static rpipeline_stage_t ssao_blur_stage;
static rpipeline_shader_t ssao_blur_shader;

static float random_float() {
    return (float)rand() / (float)(RAND_MAX);
}

static float lerp(float a, float b, float f) {
    return a + f * (b - a);
}  

static void s_ssao_kernels_init() {
    for (uint32_t i = 0; i < KERNEL_COUNT; ++i) {
        kernels.k[i] = vector4_t( random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f, random_float(), 0.0f );

        kernels.k[i] = glm::normalize(kernels.k[i]);
        kernels.k[i] *= random_float();
        float scale = (float)i / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        kernels.k[i] *= scale;
    }

    for (uint32_t i = 0; i < 16; ++i) {
        noise[i] = vector4_t(random_float(), random_float(), random_float(), 1.0f);
    }

    noise_texture = create_texture(NULL, VK_FORMAT_R16G16B16A16_SFLOAT, noise, 4, 4, VK_FILTER_NEAREST);
}

static void s_ssao_render_pass_init() {
    kernels.resolution_coefficient = 2.0f;
    ssao_stage.color_attachment_count = 1;

    VkAttachmentDescription attachment_description = r_fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16_SFLOAT);
    
    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = ssao_stage.color_attachment_count;
    subpass_description.pColorAttachments = &attachment_reference;
    subpass_description.pDepthStencilAttachment = NULL;

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
    render_pass_info.attachmentCount = ssao_stage.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &ssao_stage.render_pass));

    ssao_blur_stage.color_attachment_count = 1;
    
    attachment_description = r_fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16_SFLOAT);
    
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = ssao_blur_stage.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &ssao_blur_stage.render_pass));
}

static void s_ssao_init() {
    ssao_stage.color_attachments = FL_MALLOC(attachment_t, ssao_stage.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    extent3d.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);
    extent3d.depth = 1;

    VkExtent2D extent2d = {};
    extent2d.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    extent2d.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);

    // Can optimise memory usage by using R8G8B8A8_UNORM
    ssao_stage.color_attachments[0] = r_create_color_attachment(extent3d, VK_FORMAT_R16_SFLOAT);

    ssao_stage.framebuffer = r_create_framebuffer(
        ssao_stage.color_attachment_count,
        ssao_stage.color_attachments,
        NULL,
        ssao_stage.render_pass,
        extent2d,
        1);

    r_rpipeline_descriptor_set_output_init(&ssao_stage);

    VkDescriptorSetLayout input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, deferred.binding_count),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    ssao_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/ssao.vert.spv",
        "../shaders/SPV/ssao.frag.spv",
        &ssao_stage,
        pipeline_layout);

    kernel_uniform_buffer = create_gpu_buffer(
        sizeof(kernels),
        &kernels,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    kernel_uniform_buffer_set = create_buffer_descriptor_set(
        kernel_uniform_buffer.buffer,
        kernel_uniform_buffer.size,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    
    ssao_blur_stage.color_attachments = FL_MALLOC(attachment_t, ssao_blur_stage.color_attachment_count);

    // Can optimise memory usage by using R8G8B8A8_UNORM
    ssao_blur_stage.color_attachments[0] = r_create_color_attachment(extent3d, VK_FORMAT_R16_SFLOAT);

    ssao_blur_stage.framebuffer = r_create_framebuffer(
        ssao_blur_stage.color_attachment_count,
        ssao_blur_stage.color_attachments,
        NULL,
        ssao_blur_stage.render_pass,
        extent2d,
        1);

    r_rpipeline_descriptor_set_output_init(&ssao_blur_stage);

    VkDescriptorSetLayout blur_input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(blur_input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = blur_input_layouts;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    ssao_blur_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/ssao_blur.vert.spv",
        "../shaders/SPV/ssao_blur.frag.spv",
        &ssao_blur_stage,
        pipeline_layout);
}

void r_execute_ssao_pass(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values = {};
    
    VkRect2D render_area = {};
    render_area.extent.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    render_area.extent.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = ssao_stage.framebuffer;
    begin_info.renderPass = ssao_stage.render_pass;
    begin_info.clearValueCount = ssao_stage.color_attachment_count;
    begin_info.pClearValues = &clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    viewport.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    rect.extent.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_shader.pipeline);

    VkDescriptorSet inputs[] = {
        deferred.descriptor_set,
        r_camera_transforms_uniform(),
        noise_texture.descriptor,
        kernel_uniform_buffer_set
    };
    
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ssao_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

void r_execute_ssao_blur_pass(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values = {};
    
    VkRect2D render_area = {};
    render_area.extent.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    render_area.extent.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = ssao_blur_stage.framebuffer;
    begin_info.renderPass = ssao_blur_stage.render_pass;
    begin_info.clearValueCount = ssao_blur_stage.color_attachment_count;
    begin_info.pClearValues = &clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    viewport.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent.width = (uint32_t)((float)r_swapchain_extent().width / kernels.resolution_coefficient);
    rect.extent.height = (uint32_t)((float)r_swapchain_extent().height / kernels.resolution_coefficient);
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_blur_shader.pipeline);

    VkDescriptorSet inputs[] = {
        ssao_stage.descriptor_set
    };
    
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        ssao_blur_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

static rpipeline_stage_t lighting_stage;
static VkDescriptorSet bright_colors_set;
static rpipeline_shader_t lighting_shader;

static void s_lighting_render_pass_init() {
    lighting_stage.color_attachment_count = 2;

    VkAttachmentDescription attachment_descriptions[2] = {
        r_fill_color_attachment_description(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_FORMAT_R16G16B16A16_SFLOAT),
        r_fill_color_attachment_description(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_FORMAT_R16G16B16A16_SFLOAT) };
    
    VkAttachmentReference attachment_references[2] = {};
    attachment_references[0].attachment = 0;
    attachment_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_references[1].attachment = 1;
    attachment_references[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = lighting_stage.color_attachment_count;
    subpass_description.pColorAttachments = attachment_references;
    subpass_description.pDepthStencilAttachment = NULL;

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
    render_pass_info.attachmentCount = lighting_stage.color_attachment_count;
    render_pass_info.pAttachments = attachment_descriptions;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &lighting_stage.render_pass));
}

static void s_lighting_init() {
    lighting_stage.color_attachments = FL_MALLOC(attachment_t, lighting_stage.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = r_swapchain_extent().width;
    extent3d.height = r_swapchain_extent().height;
    extent3d.depth = 1;
    
    lighting_stage.color_attachments[0] = r_create_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);
    lighting_stage.color_attachments[1] = r_create_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    lighting_stage.framebuffer = r_create_framebuffer(
        lighting_stage.color_attachment_count,
        lighting_stage.color_attachments,
        NULL,
        lighting_stage.render_pass,
        r_swapchain_extent(),
        1);

    r_rpipeline_descriptor_set_output_init(&lighting_stage);

    bright_colors_set = create_image_descriptor_set(
        lighting_stage.color_attachments[1].image_view,
        lighting_stage.color_attachments[1].sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    VkDescriptorSetLayout input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, deferred.binding_count),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    lighting_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/lighting.vert.spv",
        "../shaders/SPV/lighting.frag.spv",
        &lighting_stage,
        pipeline_layout);
}

void r_execute_lighting_pass(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values = {};
    
    VkRect2D render_area = {};
    render_area.extent = r_swapchain_extent();

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = lighting_stage.framebuffer;
    begin_info.renderPass = lighting_stage.render_pass;
    begin_info.clearValueCount = lighting_stage.color_attachment_count;
    begin_info.pClearValues = &clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_shader.pipeline);

    VkDescriptorSet inputs[] = {
        deferred.descriptor_set,
        /* Contains lighting information */
        r_lighting_uniform(),
        /* Contains camera information */
        r_camera_transforms_uniform(),
        r_diffuse_ibl_irradiance(),
        r_integral_lookup(),
        r_specular_ibl(),
        ssao_blur_stage.descriptor_set,
        shadow_stage.descriptor_set
    };
    
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        lighting_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

static rpipeline_shader_t motion_blur_shader;
static rpipeline_stage_t motion_blur_stage;

static void s_motion_blur_render_pass_init() {
    motion_blur_stage.color_attachment_count = 1;
    
    VkAttachmentDescription attachment_description = r_fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = motion_blur_stage.color_attachment_count;
    subpass_description.pColorAttachments = &attachment_reference;
    subpass_description.pDepthStencilAttachment = NULL;

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
    render_pass_info.attachmentCount = motion_blur_stage.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &motion_blur_stage.render_pass));
}

static void s_motion_blur_init() {
    motion_blur_stage.color_attachments = FL_MALLOC(attachment_t, motion_blur_stage.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = r_swapchain_extent().width;
    extent3d.height = r_swapchain_extent().height;
    extent3d.depth = 1;

    // Can optimise memory usage by using R8G8B8A8_UNORM
    motion_blur_stage.color_attachments[0] = r_create_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    motion_blur_stage.framebuffer = r_create_framebuffer(
        motion_blur_stage.color_attachment_count,
        motion_blur_stage.color_attachments,
        NULL,
        motion_blur_stage.render_pass,
        r_swapchain_extent(),
        1);

    r_rpipeline_descriptor_set_output_init(&motion_blur_stage);

    VkDescriptorSetLayout input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, deferred.binding_count),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lighting_stage.binding_count),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    motion_blur_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/motion_blur.vert.spv",
        "../shaders/SPV/motion_blur.frag.spv",
        &motion_blur_stage,
        pipeline_layout);
}

void r_execute_motion_blur_pass(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values = {};
    
    VkRect2D render_area = {};
    render_area.extent = r_swapchain_extent();

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = motion_blur_stage.framebuffer;
    begin_info.renderPass = motion_blur_stage.render_pass;
    begin_info.clearValueCount = motion_blur_stage.color_attachment_count;
    begin_info.pClearValues = &clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, motion_blur_shader.pipeline);

    VkDescriptorSet inputs[] = {
        deferred.descriptor_set,
        r_camera_transforms_uniform(),
        lighting_stage.descriptor_set,
        r_lighting_uniform(),
    };
    
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        motion_blur_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}

static rpipeline_shader_t blur_shader;
static rpipeline_stage_t blur_stage;
VkDescriptorSet blur_sets[2];
VkDescriptorSet current_set;

static void s_blur_init() {
    blur_stage.color_attachment_count = 1;
    // Need 2 attachments for ping-pong rendering, but render pass only takes one
    blur_stage.color_attachments = FL_MALLOC(attachment_t, 2);

    VkExtent3D extent3d = {};
    extent3d.width = r_swapchain_extent().width;
    extent3d.height = r_swapchain_extent().height;
    extent3d.depth = 1;
    
    blur_stage.color_attachments[0] = r_create_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);
    blur_stage.color_attachments[1] = r_create_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    VkAttachmentDescription attachment_description = r_fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = blur_stage.color_attachment_count;
    subpass_description.pColorAttachments = &attachment_reference;
    subpass_description.pDepthStencilAttachment = NULL;

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
    render_pass_info.attachmentCount = blur_stage.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &blur_stage.render_pass));

    blur_stage.framebuffers[0] = r_create_framebuffer(
        blur_stage.color_attachment_count,
        blur_stage.color_attachments,
        NULL,
        blur_stage.render_pass,
        r_swapchain_extent(),
        1);

    blur_stage.framebuffers[1] = r_create_framebuffer(
        blur_stage.color_attachment_count,
        blur_stage.color_attachments + 1,
        NULL,
        blur_stage.render_pass,
        r_swapchain_extent(),
        1);

    //r_rpipeline_descriptor_set_output_init(&blur_stage);
    blur_sets[0] = create_image_descriptor_set(
        blur_stage.color_attachments[0].image_view,
        blur_stage.color_attachments[0].sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    current_set = blur_sets[1] = create_image_descriptor_set(
        blur_stage.color_attachments[1].image_view,
        blur_stage.color_attachments[1].sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    VkDescriptorSetLayout input_layouts[] = {
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };

    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = 4;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &range;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);
    
    blur_shader = s_create_rendering_pipeline_shader(
        "../shaders/SPV/gauss_blur.vert.spv",
        "../shaders/SPV/gauss_blur.frag.spv",
        &blur_stage,
        pipeline_layout);
}

// Blur bright colors
void r_execute_bloom_pass(
    VkCommandBuffer command_buffer) {
    VkDescriptorSet inputs[] = {
        bright_colors_set
    };

    uint32_t horizontal = true;
    
    for (uint32_t i = 0; i < 10; ++i) {
        if (i > 0) {
            inputs[0] = blur_sets[!horizontal];
            current_set = blur_sets[horizontal];
        }
        
        VkClearValue clear_values = {};
    
        VkRect2D render_area = {};
        render_area.extent = r_swapchain_extent();

        VkRenderPassBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.framebuffer = blur_stage.framebuffers[horizontal];
        begin_info.renderPass = blur_stage.render_pass;
        begin_info.clearValueCount = blur_stage.color_attachment_count;
        begin_info.pClearValues = &clear_values;
        begin_info.renderArea = render_area;

        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
        VkViewport viewport = {};
        viewport.width = r_swapchain_extent().width;
        viewport.height = r_swapchain_extent().height;
        viewport.maxDepth = 1;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D rect = {};
        rect.extent = r_swapchain_extent();
        vkCmdSetScissor(command_buffer, 0, 1, &rect);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_shader.pipeline);

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            blur_shader.layout,
            0,
            sizeof(inputs) / sizeof(VkDescriptorSet),
            inputs,
            0,
            NULL);

        vkCmdPushConstants(
            command_buffer,
            blur_shader.layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(uint32_t),
            &horizontal);

        vkCmdDraw(command_buffer, 4, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
        
        horizontal = !horizontal;
    }
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
        r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
                                             //r_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lighting_stage.binding_count),
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

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, final_shader.pipeline);

    VkDescriptorSet inputs[] = {
                                //lighting_stage.descriptor_set,
                                        motion_blur_stage.descriptor_set
                                //ssao_blur_stage.descriptor_set
                                //shadow_stage.descriptor_set
        //current_set
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
    s_shadow_init();
    
    s_deferred_render_pass_init();
    s_deferred_init();
    
    s_ssao_kernels_init();
    s_ssao_render_pass_init();
    s_ssao_init();

    s_lighting_render_pass_init();
    s_lighting_init();
    
    //s_blur_init();

    s_motion_blur_render_pass_init();
    s_motion_blur_init();

    s_final_init();
}

void begin_scene_rendering(
    VkCommandBuffer command_buffer) {
    VkClearValue clear_values[5] = {};
    
    memset(clear_values, 0, sizeof(VkClearValue) * 4);

    clear_values[4].depthStencil.depth = 1.0f;

    VkRect2D render_area = {};
    render_area.extent = r_swapchain_extent();

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = deferred.framebuffer;
    begin_info.renderPass = deferred.render_pass;
    begin_info.clearValueCount = deferred.color_attachment_count + 1;
    begin_info.pClearValues = clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

void end_scene_rendering(
    VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}

void r_handle_resize(
    uint32_t width,
    uint32_t height) {
    // Destroy everything
    destroy_rpipeline_stage(&deferred);
    destroy_rpipeline_stage(&ssao_stage);
    destroy_rpipeline_stage(&ssao_blur_stage);
    destroy_rpipeline_stage(&lighting_stage);
    destroy_rpipeline_stage(&motion_blur_stage);
    
    s_deferred_init();
    s_ssao_init();
    s_lighting_init();
    s_motion_blur_init();
}
