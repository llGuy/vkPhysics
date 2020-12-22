#pragma once

#include <vulkan/vulkan.h>

namespace vk {

void init_synchronisation();

// Utility functions for creating things like pipeline barriers, etc...
VkAccessFlags find_access_flags_for_stage(VkPipelineStageFlags stage);
VkAccessFlags find_access_flags_for_image_layout(VkImageLayout layout);
VkPipelineStageFlags find_pipeline_stage_for_image_layout(VkImageLayout layout);


VkBufferMemoryBarrier create_gpu_buffer_barrier(
    VkPipelineStageFlags src,
    VkPipelineStageFlags dst,
    uint32_t offset,
    uint32_t max,
    struct gpu_buffer_t *buffer);

VkImageMemoryBarrier create_image_barrier(
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkImage image,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_mip,
    uint32_t mip_levels,
    VkImageAspectFlags aspect);

}
