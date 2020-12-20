#include "vk_memory.hpp"
#include "vk_context.hpp"
#include "vk_render_pipeline.hpp"

namespace vk {

void fill_main_inheritance_info(
    VkCommandBufferInheritanceInfo *info,
    stage_type_t type) {
    render_pipeline_stage_t *stage;
    
    switch (type) {
    default: case ST_DEFERRED: {
        stage = r_deferred_stage();
    } break;

    case ST_SHADOW: {
        stage = r_shadow_stage();
    } break;

    case ST_POST_PROCESS: {
        stage = r_motion_blur_stage();
    } break;

    case ST_UI: {
        stage = r_ui_stage();
    } break;
    }

    info->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    info->renderPass = stage->render_pass;
    info->subpass = 0;
    info->framebuffer = stage->framebuffer;
}

void attachment_t::init_as_color_attachment(
    VkExtent3D extent, 
    VkFormat a_format) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = a_format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(g_ctx->device, &image_info, NULL, &image);

    image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = a_format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(g_ctx->device, &image_view_info, NULL, &image_view));

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // Default to linear - TODO: Add filter type as parameter in future when needed
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(g_ctx->device, &sampler_info, NULL, &sampler));

    format = a_format;
}

void attachment_t::init_as_depth_attachment(VkExtent3D extent) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = g_ctx->suitable_hardware_depth_format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(g_ctx->device, &image_info, NULL, &image);

    image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = g_ctx->suitable_hardware_depth_format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(g_ctx->device, &image_view_info, NULL, &image_view));

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(g_ctx->device, &sampler_info, NULL, &sampler));

    format = g_ctx->suitable_hardware_depth_format;
}

VkFramebuffer create_framebuffer(
    uint32_t color_attachment_count,
    attachment_t *color_attachments, 
    attachment_t *depth_attachment,
    VkRenderPass render_pass,
    VkExtent2D extent,
    uint32_t output_layer_count) {
    uint32_t attachment_count = color_attachment_count + (depth_attachment ? 1 : 0);
    VkImageView *attachments = flmalloc<VkImageView>(attachment_count);
    for (uint32_t i = 0; i < color_attachment_count; ++i) {
        attachments[i] = color_attachments[i].image_view;
    }
    if (depth_attachment) {
        attachments[color_attachment_count] = depth_attachment->image_view;
    }

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.attachmentCount = attachment_count;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = output_layer_count;
    framebuffer_info.renderPass = render_pass;

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(g_ctx->device, &framebuffer_info, NULL, &framebuffer);

    flfree(attachments);

    return framebuffer;
}

void render_pipeline_stage_t::init_descriptor_set_output(bool add_depth) {
    VkImageView *views = ALLOCA(VkImageView, color_attachment_count + 1);
    VkSampler *samplers = ALLOCA(VkSampler, color_attachment_count + 1);
    
    uint32_t binding_count = 0;

    for (; binding_count < color_attachment_count; ++binding_count) {
        views[binding_count] = color_attachments[binding_count].image_view;
        samplers[binding_count] = color_attachments[binding_count].sampler;
    }

    if (depth_attachment && add_depth) {
        views[binding_count] = depth_attachment->image_view;
        samplers[binding_count] = depth_attachment->sampler;
        
        ++binding_count;
    }

    binding_count = binding_count;

    descriptor_set = create_image_descriptor_set(
        views,
        samplers,
        binding_count,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

VkAttachmentDescription fill_color_attachment_description(VkImageLayout layout, VkFormat format) {
    VkAttachmentDescription description = {};
    description.finalLayout = layout;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    description.format = format;
    description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.samples = VK_SAMPLE_COUNT_1_BIT;

    return description;
}

VkAttachmentDescription fill_depth_attachment_description(VkImageLayout layout) {
    VkAttachmentDescription description = {};
    description.finalLayout = layout;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    description.format = g_ctx->suitable_hardware_depth_format;
    description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.samples = VK_SAMPLE_COUNT_1_BIT;

    return description;
}


}
