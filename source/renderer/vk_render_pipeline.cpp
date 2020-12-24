#include "vk_memory.hpp"
#include "vk_context.hpp"
#include "vk_stage_ui.hpp"
#include "vk_stage_post.hpp"
#include "vk_stage_blur.hpp"
#include "vk_stage_final.hpp"
#include "vk_stage_shadow.hpp"
#include "vk_stage_deferred.hpp"
#include "vk_stage_lighting.hpp"
#include "vk_render_pipeline.hpp"

#include <vk.hpp>

namespace vk {

void fill_main_inheritance_info(
    VkCommandBufferInheritanceInfo *info,
    stage_type_t type) {
    render_pipeline_stage_t *stage;
    
    switch (type) {
    default: case ST_DEFERRED: {
        stage = &g_ctx->pipeline.deferred->stage;
    } break;

    case ST_SHADOW: {
        stage = &g_ctx->pipeline.shadow->stage;
    } break;

    case ST_POST_PROCESS: {
        stage = &g_ctx->pipeline.post->stage;
    } break;

    case ST_UI: {
        stage = &g_ctx->pipeline.ui->stage;
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

void attachment_t::init_as_cubemap(
    VkExtent3D extent,
    VkFormat c_format,
    uint32_t mip_levels,
    VkSamplerAddressMode address_mode) {
    format = c_format;

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.arrayLayers = 6;
    image_info.extent = extent;
    image_info.format = format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = mip_levels;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(g_ctx->device, &image_info, NULL, &image));

    image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    image_view_info.format = format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = mip_levels;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 6;

    VK_CHECK(vkCreateImageView(g_ctx->device, &image_view_info, NULL, &image_view));

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.maxLod = (float)mip_levels;
    
    VK_CHECK(vkCreateSampler(g_ctx->device, &sampler_info, NULL, &sampler));
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
    
    binding_count = 0;

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

void render_pipeline_stage_t::free() {
    if (color_attachments) {
        flfree(color_attachments);
        color_attachments = NULL;
    }
    if (depth_attachment) {
        flfree(depth_attachment);
        depth_attachment = NULL;
    }
}

void render_pipeline_stage_t::destroy() {
    vkDestroyFramebuffer(g_ctx->device, framebuffer, NULL);
    for (uint32_t i = 0; i < color_attachment_count; ++i) {
        vkDestroyImageView(g_ctx->device, color_attachments[i].image_view, NULL);
        vkDestroySampler(g_ctx->device, color_attachments[i].sampler, NULL);
        vkFreeMemory(g_ctx->device, color_attachments[i].image_memory, NULL);
        vkDestroyImage(g_ctx->device, color_attachments[i].image, NULL);
    }

    if (depth_attachment) {
        vkDestroyImageView(g_ctx->device, depth_attachment->image_view, NULL);
        vkDestroySampler(g_ctx->device, depth_attachment->sampler, NULL);
        vkFreeMemory(g_ctx->device, depth_attachment->image_memory, NULL);
        vkDestroyImage(g_ctx->device, depth_attachment->image, NULL);
        FL_FREE(depth_attachment);
        depth_attachment = NULL;
    }

    if (descriptor_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(g_ctx->device, g_ctx->descriptor_pool, 1, &descriptor_set);
    }

    if (color_attachments) {
        FL_FREE(color_attachments);
        color_attachments = NULL;
    }
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

void update_pipeline_previous_output(VkDescriptorSet output) {
    g_ctx->pipeline.previous_output = output;
}

render_pipeline_stage_t *get_stage_before_final_render() {
    return &g_ctx->pipeline.post->stage;
}

void render_pipeline_t::allocate() {
    shadow = flmalloc<shadow_stage_t>();
    deferred = flmalloc<deferred_stage_t>();
    lighting = flmalloc<lighting_stage_t>();
    ui = flmalloc<ui_stage_t>();
    blur = flmalloc<blur_stage_t>();
    post = flmalloc<post_stage_t>();
    fin = flmalloc<final_stage_t>();
}

void render_pipeline_t::init() {
    shadow->init();

    deferred->init_render_pass();
    deferred->init();

    lighting->init_render_pass();
    lighting->init();

    { // Initialise scene blur
        memset(blur, 0, sizeof(blur_stage_t));

        blur->init(
            {g_ctx->swapchain.extent.width / 2, g_ctx->swapchain.extent.height / 2},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            BKS_9);
    }

    post->init_render_pass();
    post->init();

    ui->init_render_pass();
    ui->init();

    fin->init();
}

void render_pipeline_t::resize() {
    deferred->stage.destroy();
    lighting->stage.destroy();
    post->stage.destroy();
    blur->stage.destroy();
    ui->stage.destroy();
    
    deferred->init();
    lighting->init();
    post->init();

    { // Reinitialise blur
        memset(blur, 0, sizeof(blur_stage_t));

        blur->init(
            {g_ctx->swapchain.extent.width / 2, g_ctx->swapchain.extent.height / 2},
            VK_FORMAT_R16G16B16A16_SFLOAT,
            BKS_9);
    }

    ui->init();
}

void render_pipeline_t::deallocate() {
    flfree(shadow);
    flfree(deferred);
    flfree(lighting);
    flfree(ui);
    flfree(blur);
    flfree(post);
    flfree(fin);
}

void post_process_scene(frame_info_t *info, VkCommandBuffer ui_cmdbuf) {
    g_ctx->pipeline.lighting->execute(g_ctx->primary_command_buffers[g_ctx->image_index]);
    g_ctx->pipeline.post->execute(g_ctx->primary_command_buffers[g_ctx->image_index], ui_cmdbuf);

    if (info->blurred) {
        // Pass this to whatever the last pass is
        float blur_output_width = (float)(g_ctx->swapchain.extent.width / 2);
        float blur_output_height = (float)(g_ctx->swapchain.extent.height / 2);

        g_ctx->pipeline.blur->execute(
            g_ctx->pipeline.post->stage.descriptor_set,
            1.0f / vector2_t(blur_output_width, blur_output_height),
            g_ctx->primary_command_buffers[g_ctx->image_index]);
    }

    g_ctx->pipeline.ui->execute(g_ctx->primary_command_buffers[g_ctx->image_index], ui_cmdbuf);
}

void resize_render_pipeline(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(g_ctx->device);

    resize_swapchain(width, height);
    g_ctx->pipeline.resize();
}

}
