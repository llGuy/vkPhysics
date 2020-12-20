#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace vk {

struct gpu_buffer_t {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkDeviceSize size;

    void init(
        uint32_t size,
        void *data,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_flags);

    void update(
        VkCommandBuffer command_buffer,
        VkPipelineStageFlags pipeline_stage,
        uint32_t offset,
        uint32_t data_size,
        void *data);

    void destroy();
};

struct sensitive_gpu_buffer_t {
    uint64_t frame_id;
    gpu_buffer_t buffer;
};

constexpr uint32_t MAX_DELETIONS = 1000;

void init_sensitive_buffer_deletions();
void destroy_sensitive_buffer(gpu_buffer_t *buf);

void update_sensitive_buffer_deletions();

}
