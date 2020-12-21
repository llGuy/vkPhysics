#include "vk_context.hpp"
#include "vk_stage_final.hpp"

#include <common/allocators.hpp>

namespace vk {

void final_stage_t::init() {
    main_screen_brightness = 0.0f;

    stage.color_attachment_count = 1;
    stage.color_attachments = FL_MALLOC(attachment_t, 1);
    stage.color_attachments[0].format = g_ctx->swapchain.format;
    stage.depth_attachment = NULL;
    stage.render_pass = g_ctx->swapchain.final_pass;
    
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
    range.size = sizeof(float); // Just to hold brightness value for now
    pipeline_layout_info.pPushConstantRanges = &range;
    
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(g_ctx->device, &pipeline_layout_info, NULL, &pipeline_layout);
    
    shader.init_as_render_pipeline_shader(
        "shaders/SPV/final.vert.spv",
        "shaders/SPV/final.frag.spv",
        &stage,
        pipeline_layout);
}

void final_stage_t::execute(VkCommandBuffer cmdbuf) {
       // brightness is for fade effect
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipeline);

    VkDescriptorSet inputs[] = {
        g_ctx->pipeline.previous_output,
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

    vkCmdPushConstants(
        cmdbuf,
        shader.layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(float),
        &main_screen_brightness);

    vkCmdDraw(cmdbuf, 4, 1, 0, 0);
}

void set_main_screen_brightness(float brightness) {
    g_ctx->pipeline.fin->main_screen_brightness = brightness;
}

}
