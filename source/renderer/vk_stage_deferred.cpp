#include "vk_debug.hpp"
#include "vk_context.hpp"
#include "vk_stage_deferred.hpp"

#include <allocators.hpp>

namespace vk {

void deferred_stage_t::init_render_pass() {
    stage.color_attachment_count = 4;

    VkAttachmentDescription *attachment_descriptions = FL_MALLOC(VkAttachmentDescription, stage.color_attachment_count + 1);
    for (uint32_t i = 0; i < stage.color_attachment_count; ++i) {
        attachment_descriptions[i] = fill_color_attachment_description(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    attachment_descriptions[stage.color_attachment_count] = fill_depth_attachment_description(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkAttachmentReference *attachment_references = FL_MALLOC(VkAttachmentReference, stage.color_attachment_count + 1);
    for (uint32_t i = 0; i < stage.color_attachment_count; ++i) {
        attachment_references[i].attachment = i;
        attachment_references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    attachment_references[stage.color_attachment_count].attachment = stage.color_attachment_count;
    attachment_references[stage.color_attachment_count].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = stage.color_attachment_count;
    subpass_description.pColorAttachments = attachment_references;
    subpass_description.pDepthStencilAttachment = &attachment_references[stage.color_attachment_count];

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
    render_pass_info.attachmentCount = stage.color_attachment_count + 1;
    render_pass_info.pAttachments = attachment_descriptions;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &stage.render_pass));

    FL_FREE(attachment_descriptions);
    FL_FREE(attachment_references);
}

void deferred_stage_t::init() {
    stage.color_attachments = FL_MALLOC(attachment_t, stage.color_attachment_count);
    stage.depth_attachment = FL_MALLOC(attachment_t, 1);

    VkExtent2D swapchain_extent = g_ctx->swapchain.extent;

    VkExtent3D extent = {};
    extent.width = swapchain_extent.width;
    extent.height = swapchain_extent.height;
    extent.depth = 1;

    for (uint32_t i = 0; i < stage.color_attachment_count; ++i) {
        stage.color_attachments[i].init_as_color_attachment(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    stage.depth_attachment->init_as_depth_attachment(extent);
    
    stage.framebuffer = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments,
        stage.depth_attachment,
        stage.render_pass,
        swapchain_extent,
        1);

    stage.init_descriptor_set_output();
}

void begin_scene_rendering(VkCommandBuffer command_buffer) {
    begin_debug_region(command_buffer, "Deferred Rendering Stage", STAGE_COLORS[ST_DEFERRED]);

    VkClearValue clear_values[5] = {};
    
    memset(clear_values, 0, sizeof(VkClearValue) * 4);

    clear_values[4].depthStencil.depth = 1.0f;

    VkRect2D render_area = {};
    render_area.extent = g_ctx->swapchain.extent;

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = g_ctx->pipeline.deferred->stage.framebuffer;
    begin_info.renderPass = g_ctx->pipeline.deferred->stage.render_pass;
    begin_info.clearValueCount = g_ctx->pipeline.deferred->stage.color_attachment_count + 1;
    begin_info.pClearValues = clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

void end_scene_rendering(VkCommandBuffer cmdbuf) {
    vkCmdEndRenderPass(cmdbuf);
    end_debug_region(cmdbuf);
}

}
