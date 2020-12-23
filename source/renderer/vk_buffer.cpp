#include "vk_sync.hpp"
#include "vk_buffer.hpp"
#include "vk_memory.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_context.hpp"

namespace vk {

void init_sensitive_buffer_deletions() {
    g_ctx->buffer_deletions.init(MAX_DELETIONS);
}

void destroy_sensitive_buffer(gpu_buffer_t *buf) {
    uint32_t i = g_ctx->buffer_deletions.add();
    g_ctx->buffer_deletions[i].buffer = *buf;
    g_ctx->buffer_deletions[i].frame_id = g_ctx->frame_id;
}

void update_sensitive_buffer_deletions() {
    for (int32_t i = g_ctx->buffer_deletions.data_count - 1; i >= 0; --i) {
        sensitive_gpu_buffer_t *b = g_ctx->buffer_deletions.get(i);
        if (b->frame_id + 3 <= g_ctx->frame_id) {
            // Delete the buffer
            b->buffer.destroy();
            g_ctx->buffer_deletions.remove(i);
        }
    }
}

void gpu_buffer_t::init(
    uint32_t buf_size,
    void *data,
    VkBufferUsageFlags buf_usage,
    VkMemoryPropertyFlags memory_flags) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buf_size;
    buffer_info.usage = data ? buf_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT : buf_usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(g_ctx->device, &buffer_info, NULL, &buffer);

    memory = allocate_gpu_buffer_memory(
        buffer,
        memory_flags);

    usage = buf_usage;
    size = (VkDeviceSize)buf_size;

    if (data) {
        // Create staging buffer
        VkBufferCreateInfo staging_buffer_info = {};
        staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_info.size = size;
        staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer staging_buffer;
        vkCreateBuffer(g_ctx->device, &staging_buffer_info, NULL, &staging_buffer);

        VkDeviceMemory staging_memory = allocate_gpu_buffer_memory(
            staging_buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void *staging_map;
        vkMapMemory(g_ctx->device, staging_memory, 0, size, 0, &staging_map);

        memcpy(staging_map, data, size);

        vkUnmapMemory(g_ctx->device, staging_memory);
        
        VkCommandBuffer command_buffer = begin_single_use_command_buffer();
        
        VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            size,
            this);

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            1, &barrier,
            0, NULL);

        VkBufferCopy copy_region = {};
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, staging_buffer, buffer, 1, &copy_region);

        barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            size,
            this);

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, NULL,
            1, &barrier,
            0, NULL);

        end_single_use_command_buffer(command_buffer);

        vkFreeMemory(g_ctx->device, staging_memory, NULL);
        vkDestroyBuffer(g_ctx->device, staging_buffer, NULL);
    }
}
 
void gpu_buffer_t::update(
    VkCommandBuffer command_buffer,
    VkPipelineStageFlags pipeline_stage,
    uint32_t offset,
    uint32_t data_size,
    void *data) {
    VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
        pipeline_stage,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        offset,
        data_size,
        this);

    vkCmdPipelineBarrier(
        command_buffer,
        pipeline_stage,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);

    vkCmdUpdateBuffer(
        command_buffer,
        buffer,
        offset,
        data_size,
        data);

    barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        pipeline_stage,
        offset,
        data_size,
        this);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        pipeline_stage,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);
}

void gpu_buffer_t::destroy() {
    vkFreeMemory(g_ctx->device, memory, NULL);
    vkDestroyBuffer(g_ctx->device, buffer, NULL);
}

}
