#include "vk_context.hpp"
#include "vk_stage_shadow.hpp"

#include <allocators.hpp>

namespace vk {

void shadow_stage_t::init() {
    map_extent.width = 1024;
    map_extent.height = 1024;

    VkExtent3D extent3d = {};
    extent3d.width = map_extent.width;
    extent3d.height = map_extent.height;
    extent3d.depth = 1;
    
    stage.color_attachment_count = 1;
    stage.color_attachments = flmalloc<attachment_t>(1);
    stage.color_attachments[0].init_as_color_attachment(extent3d, VK_FORMAT_R32G32_SFLOAT);
    stage.depth_attachment = flmalloc<attachment_t>(1);
    stage.depth_attachment->init_as_depth_attachment(extent3d);

    VkAttachmentDescription attachment_descriptions[2] = {};
    attachment_descriptions[0] = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R32G32_SFLOAT);

    attachment_descriptions[1] = fill_depth_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    VkAttachmentReference color_attachment_reference = {};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_reference = {};
    depth_attachment_reference.attachment = 1;
    depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_reference;
    subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachment_descriptions;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &stage.render_pass));

    stage.framebuffer = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments,
        stage.depth_attachment,
        stage.render_pass,
        map_extent,
        1);

    stage.init_descriptor_set_output(0);

    blur.init(
        {map_extent.width, map_extent.height},
        VK_FORMAT_R32G32_SFLOAT,
        BKS_7);
}

void begin_shadow_rendering(VkCommandBuffer cmdbuf) {
    shadow_stage_t *shadow = g_ctx->pipeline.shadow;

    VkClearValue clear_values[2] = {};
    clear_values[0].color.float32[0] = 1.0f;
    clear_values[0].color.float32[1] = 1.0f;
    clear_values[1].depthStencil.depth = 1.0f;

    VkRect2D render_area = {};
    render_area.extent = shadow->map_extent;

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.framebuffer = shadow->stage.framebuffer;
    begin_info.renderPass = shadow->stage.render_pass;
    begin_info.clearValueCount = 2;
    begin_info.pClearValues = clear_values;
    begin_info.renderArea = render_area;

    vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

void end_shadow_rendering(VkCommandBuffer cmdbuf) {
    shadow_stage_t *shadow = g_ctx->pipeline.shadow;

    vkCmdEndRenderPass(cmdbuf);

    float blur_output_width = (float)(shadow->map_extent.width / 2);
    float blur_output_height = (float)(shadow->map_extent.height / 2);
    vector2_t scale = 1.0f / vector2_t(blur_output_width, blur_output_height);

    shadow->blur.execute(
        shadow->stage.descriptor_set,
        scale,
        cmdbuf);
}

}
