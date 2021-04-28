#include "vk_render_pipeline.hpp"
#include "vk_shader.hpp"
#include "vk_context.hpp"
#include "vk_stage_blur.hpp"
#include "vk_descriptor.hpp"

#include <allocators.hpp>

namespace vk {

void blur_stage_t::init(
    VkExtent2D b_extent,
    VkFormat format,
    uint32_t kernel_size) {
    stage.color_attachment_count = 1;
    // Need 2 attachments for ping-pong rendering, but render pass only takes one
    stage.color_attachments = flmalloc<attachment_t>(2);

    extent = b_extent;

    VkExtent3D extent3d = {};
    extent3d.width = extent.width;
    extent3d.height = extent.height;
    extent3d.depth = 1;
    
    stage.color_attachments[0].init_as_color_attachment(extent3d, format);
    stage.color_attachments[1].init_as_color_attachment(extent3d, format);

    VkAttachmentDescription attachment_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        format);
    
    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = stage.color_attachment_count;
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
    render_pass_info.attachmentCount = stage.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &stage.render_pass));

    stage.framebuffers[0] = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments,
        NULL,
        stage.render_pass,
        extent,
        1);

    stage.framebuffers[1] = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments + 1,
        NULL,
        stage.render_pass,
        extent,
        1);

    sets[0] = create_image_descriptor_set(
        stage.color_attachments[0].image_view,
        stage.color_attachments[0].sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    current_set = sets[1] = create_image_descriptor_set(
        stage.color_attachments[1].image_view,
        stage.color_attachments[1].sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    stage.descriptor_set = VK_NULL_HANDLE;

    VkDescriptorSetLayout input_layouts[] = {
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };

    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = sizeof(uint32_t) + sizeof(vector2_t);
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &range;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(g_ctx->device, &pipeline_layout_info, NULL, &pipeline_layout);

    const char *frag = NULL;

    switch (kernel_size) {
    case BKS_7: {
        frag = "shaders/SPV/gauss_blur_k7.frag.spv";
    } break;

    case BKS_9: {
        frag = "shaders/SPV/gauss_blur_k9.frag.spv";
    } break;

    default: {
        LOG_ERRORV("Invalid blur kernel size: %d", kernel_size);
        assert(0);
    } break;
    }
    
    shader.init_as_render_pipeline_shader(
        "shaders/SPV/gauss_blur.vert.spv",
        frag,
        &stage,
        pipeline_layout);
}

void blur_stage_t::execute(
    VkDescriptorSet input,
    const vector2_t &scale,
    VkCommandBuffer command_buffer) {
    VkDescriptorSet inputs[] = {input};

    struct push_constant_t {
        uint32_t horizontal;
        vector2_t scale;
    } push_constant;

    push_constant.horizontal = true;
    push_constant.scale = scale;
    
    for (uint32_t i = 0; i < 2; ++i) {
        if (i > 0) {
            inputs[0] = sets[!push_constant.horizontal];
            current_set = sets[push_constant.horizontal];
        }
        
        VkClearValue clear_values = {};
    
        VkRect2D render_area = {};
        render_area.extent = extent;

        VkRenderPassBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.framebuffer = stage.framebuffers[push_constant.horizontal];
        begin_info.renderPass = stage.render_pass;
        begin_info.clearValueCount = stage.color_attachment_count;
        begin_info.pClearValues = &clear_values;
        begin_info.renderArea = render_area;

        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
        VkViewport viewport = {};
        viewport.width = (float)extent.width;
        viewport.height = (float)extent.height;
        viewport.maxDepth = 1;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D rect = {};
        rect.extent = extent;
        vkCmdSetScissor(command_buffer, 0, 1, &rect);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipeline);

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            shader.layout,
            0,
            sizeof(inputs) / sizeof(VkDescriptorSet),
            inputs,
            0,
            NULL);

        vkCmdPushConstants(
            command_buffer,
            shader.layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(push_constant_t),
            &push_constant);

        vkCmdDraw(command_buffer, 4, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
        
        push_constant.horizontal = !push_constant.horizontal;
    }

    update_pipeline_previous_output(current_set);
}

}
