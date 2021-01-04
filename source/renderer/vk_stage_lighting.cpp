#include "vk_debug.hpp"
#include "vk_scene3d.hpp"
#include "vk_context.hpp"
#include "vk_stage_shadow.hpp"
#include "vk_stage_deferred.hpp"
#include "vk_stage_lighting.hpp"

namespace vk {

void lighting_stage_t::init_render_pass() {
    stage.color_attachment_count = 2;

    VkAttachmentDescription attachment_descriptions[2] = {
        fill_color_attachment_description(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_FORMAT_R16G16B16A16_SFLOAT),
        fill_color_attachment_description(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_FORMAT_R16G16B16A16_SFLOAT) };
    
    VkAttachmentReference attachment_references[2] = {};
    attachment_references[0].attachment = 0;
    attachment_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_references[1].attachment = 1;
    attachment_references[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = stage.color_attachment_count;
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
    render_pass_info.attachmentCount = stage.color_attachment_count;
    render_pass_info.pAttachments = attachment_descriptions;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &stage.render_pass));
}

void lighting_stage_t::init() {
    stage.color_attachments = FL_MALLOC(attachment_t, stage.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = g_ctx->swapchain.extent.width;
    extent3d.height = g_ctx->swapchain.extent.height;
    extent3d.depth = 1;
    
    stage.color_attachments[0].init_as_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);
    stage.color_attachments[1].init_as_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    stage.framebuffer = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments,
        NULL,
        stage.render_pass,
        g_ctx->swapchain.extent,
        1);

    stage.init_descriptor_set_output();

    VkDescriptorSetLayout input_layouts[] = {
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, g_ctx->pipeline.deferred->stage.binding_count),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(g_ctx->device, &pipeline_layout_info, NULL, &pipeline_layout);
    
    shader.init_as_render_pipeline_shader(
        "shaders/SPV/lighting.vert.spv",
        "shaders/SPV/lighting.frag.spv",
        &stage,
        pipeline_layout);
}

void lighting_stage_t::execute(VkCommandBuffer cmdbuf) {
    begin_debug_region(cmdbuf, "Lighting Stage", STAGE_COLORS[ST_LIGHTING]);

    VkClearValue clear_values = {};
    
    VkRect2D render_area = {};
    render_area.extent = g_ctx->swapchain.extent;

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = stage.framebuffer;
    begin_info.renderPass = stage.render_pass;
    begin_info.clearValueCount = stage.color_attachment_count;
    begin_info.pClearValues = &clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipeline);

    VkDescriptorSet deferred_set = g_ctx->pipeline.deferred->stage.descriptor_set;
    scene3d_descriptors_t scene_descriptors = get_scene_descriptors();

    VkDescriptorSet inputs[] = {
        deferred_set,
        /* Contains lighting information */
        scene_descriptors.lighting_ubo,
        /* Contains camera information */
        scene_descriptors.camera_ubo,
        scene_descriptors.diff_ibl_tx,
        scene_descriptors.lut_tx,
        scene_descriptors.spec_ibl_tx,
        g_ctx->pipeline.shadow->blur.current_set
    };
    
    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(cmdbuf, 4, 1, 0, 0);

    vkCmdEndRenderPass(cmdbuf);

    end_debug_region(cmdbuf);

    update_pipeline_previous_output(stage.descriptor_set);
}

}
