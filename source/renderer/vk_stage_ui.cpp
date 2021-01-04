#include "vk_debug.hpp"
#include "vk_context.hpp"
#include "vk_stage_ui.hpp"
#include "vk_descriptor.hpp"

#include <allocators.hpp>

namespace vk {

void ui_stage_t::init_render_pass() {
    stage.color_attachment_count = 1;
    
    VkAttachmentDescription attachment_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
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
}

void ui_stage_t::init() {
    stage.color_attachments = FL_MALLOC(attachment_t, stage.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = g_ctx->swapchain.extent.width;
    extent3d.height = g_ctx->swapchain.extent.height;
    extent3d.depth = 1;

    // Can optimise memory usage by using R8G8B8A8_UNORM
    stage.color_attachments[0].init_as_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    stage.framebuffer = create_framebuffer(
        stage.color_attachment_count,
        stage.color_attachments,
        NULL,
        stage.render_pass,
        g_ctx->swapchain.extent,
        1);

    stage.init_descriptor_set_output();

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        render_previous_cmdbufs,
        3);

    render_previous_index = 0;

    VkDescriptorSetLayout input_layouts[] = {
        get_descriptor_layout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = sizeof(input_layouts) / sizeof(VkDescriptorSetLayout);
    pipeline_layout_info.pSetLayouts = input_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.size = sizeof(float); // Just to hold alpha / brightness value
    pipeline_layout_info.pPushConstantRanges = &range;
    
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(g_ctx->device, &pipeline_layout_info, NULL, &pipeline_layout);

    shader.init_as_render_pipeline_shader(
        "shaders/SPV/render_quad.vert.spv",
        "shaders/SPV/render_quad.frag.spv",
        &stage,
        pipeline_layout);
}

void ui_stage_t::execute(VkCommandBuffer cmdbuf, VkCommandBuffer ui_cmdbuf) {
    VkCommandBufferInheritanceInfo inheritance_info = {};
    fill_main_inheritance_info(
        &inheritance_info,
        ST_UI);

    VkCommandBuffer current_buffer = render_previous_cmdbufs[render_previous_index];

    begin_command_buffer(
        current_buffer,
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        &inheritance_info);

    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(current_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(current_buffer, 0, 1, &rect);

    vkCmdBindPipeline(current_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipeline);

    VkDescriptorSet inputs[] = {
        g_ctx->pipeline.previous_output,
    };
    
    vkCmdBindDescriptorSets(
        current_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0,
        NULL);

    vkCmdDraw(current_buffer, 4, 1, 0, 0);

    end_command_buffer(current_buffer);

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

    begin_debug_region(cmdbuf, "UI Stage", STAGE_COLORS[ST_UI]);
    vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    
    vkCmdExecuteCommands(cmdbuf, 1, &current_buffer);
    vkCmdExecuteCommands(cmdbuf, 1, &ui_cmdbuf);

    vkCmdEndRenderPass(cmdbuf);
    end_debug_region(cmdbuf);

    update_pipeline_previous_output(stage.descriptor_set);

    render_previous_index = (render_previous_index + 1) % 3;
}

}
