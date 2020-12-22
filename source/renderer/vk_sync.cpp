#include "vk_sync.hpp"
#include "vk_context.hpp"

#include <stdlib.h>

namespace vk {

void init_synchronisation() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(g_ctx->device, &semaphore_info, NULL, &g_ctx->image_ready_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_ctx->device, &semaphore_info, NULL, &g_ctx->render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(g_ctx->device, &fence_info, NULL, &g_ctx->fences[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create synchronisation primitives\n");
            exit(1);
        }
    }
}

VkAccessFlags find_access_flags_for_stage(VkPipelineStageFlags stage) {
    switch (stage) {
    case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT: case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT: case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT: return VK_ACCESS_UNIFORM_READ_BIT;
    case VK_PIPELINE_STAGE_TRANSFER_BIT: return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    default: exit(1); return 0;
    }
}

VkAccessFlags find_access_flags_for_image_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return 0;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    default: exit(1); return (VkAccessFlagBits)0;
    }
}

VkPipelineStageFlags find_pipeline_stage_for_image_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    default: exit(1); return (VkPipelineStageFlags)0;
    }
}

VkBufferMemoryBarrier create_gpu_buffer_barrier(
    VkPipelineStageFlags src,
    VkPipelineStageFlags dst,
    uint32_t offset,
    uint32_t max,
    gpu_buffer_t *buffer) {
    VkBufferMemoryBarrier buffer_barrier = {};
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.buffer = buffer->buffer;
    buffer_barrier.size = buffer->size;
    buffer_barrier.offset = offset;
    buffer_barrier.size = max;
    buffer_barrier.srcAccessMask = find_access_flags_for_stage(src);
    buffer_barrier.dstAccessMask = find_access_flags_for_stage(dst);

    return buffer_barrier;
}

VkImageMemoryBarrier create_image_barrier(
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkImage image,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_mip,
    uint32_t mip_levels,
    VkImageAspectFlags aspect) {
    VkImageMemoryBarrier image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = find_access_flags_for_image_layout(old_image_layout);
    image_barrier.dstAccessMask = find_access_flags_for_image_layout(new_image_layout);
    image_barrier.oldLayout = old_image_layout;
    image_barrier.newLayout = new_image_layout;
    image_barrier.image = image;
    image_barrier.subresourceRange.aspectMask = aspect;
    image_barrier.subresourceRange.baseMipLevel = base_mip;
    image_barrier.subresourceRange.levelCount = mip_levels;
    image_barrier.subresourceRange.baseArrayLayer = base_layer;
    image_barrier.subresourceRange.layerCount = layer_count;

    return image_barrier;
}


}
