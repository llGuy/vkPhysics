#pragma once

#include <vulkan/vulkan.h>

namespace vk {

VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties);

VkDeviceMemory allocate_gpu_buffer_memory(
    VkBuffer buffer,
    VkMemoryPropertyFlags properties);

void *map_gpu_memory(
    VkDeviceMemory memory,
    uint32_t size,
    uint32_t offset);

void unmap_gpu_memory(
    VkDeviceMemory memory);

}
