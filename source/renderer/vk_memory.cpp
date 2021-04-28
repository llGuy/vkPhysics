#include "vk_memory.hpp"
#include "vk_context.hpp"

#include <stdlib.h>

namespace vk {

static uint32_t s_find_memory_type_according_to_requirements(
    VkMemoryPropertyFlags properties, 
    VkMemoryRequirements memory_requirements) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(g_ctx->hardware, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (memory_requirements.memoryTypeBits & (1 << i)
            && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return(i);
        }
    }

    assert(false);

    return 0;
}

VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties) {
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(g_ctx->device, image, &requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = s_find_memory_type_according_to_requirements(properties, requirements);

    VkDeviceMemory memory;
    vkAllocateMemory(g_ctx->device, &alloc_info, nullptr, &memory);

    vkBindImageMemory(g_ctx->device, image, memory, 0);

    return memory;
}

VkDeviceMemory allocate_gpu_buffer_memory(
    VkBuffer buffer,
    VkMemoryPropertyFlags properties) {
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(g_ctx->device, buffer, &requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = s_find_memory_type_according_to_requirements(properties, requirements);

    VkDeviceMemory memory;
    vkAllocateMemory(g_ctx->device, &alloc_info, nullptr, &memory);

    vkBindBufferMemory(g_ctx->device, buffer, memory, 0);

    return memory;
}

void *map_gpu_memory(
    VkDeviceMemory memory,
    uint32_t size,
    uint32_t offset) {
    void *ptr;
    vkMapMemory(g_ctx->device, memory, offset, size, 0, &ptr);

    return ptr;
}

void unmap_gpu_memory(
    VkDeviceMemory memory) {
    vkUnmapMemory(g_ctx->device, memory);
}

}
