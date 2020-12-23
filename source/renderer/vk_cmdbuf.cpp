#include "vk_cmdbuf.hpp"
#include "vk_context.hpp"

#include <common/allocators.hpp>

namespace vk {

void init_command_pool() {
    VkCommandPoolCreateInfo cmdpool_info = {};
    cmdpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdpool_info.queueFamilyIndex = g_ctx->queue_families.graphics_family;
    cmdpool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(g_ctx->device, &cmdpool_info, NULL, &g_ctx->graphics_command_pool));
}

void init_primary_command_buffers() {
    g_ctx->primary_command_buffer_count = g_ctx->swapchain.image_count;
    g_ctx->primary_command_buffers = flmalloc<VkCommandBuffer>(g_ctx->primary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        g_ctx->primary_command_buffers,
        g_ctx->primary_command_buffer_count);
}

void create_command_buffers(
    VkCommandBufferLevel level,
    VkCommandBuffer *command_buffers, uint32_t count) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.level = level;
    allocate_info.commandPool = g_ctx->graphics_command_pool;
    allocate_info.commandBufferCount = count;

    VK_CHECK(vkAllocateCommandBuffers(g_ctx->device, &allocate_info, command_buffers));
}

void begin_command_buffer(
    VkCommandBuffer command_buffer,
    VkCommandBufferUsageFlags usage,
    VkCommandBufferInheritanceInfo *inheritance) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = usage;
    begin_info.pInheritanceInfo = inheritance;

    vkBeginCommandBuffer(command_buffer, &begin_info);
}

void end_command_buffer(
    VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);
}

VkCommandBuffer begin_single_use_command_buffer() {
    VkCommandBuffer command_buffer;
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, &command_buffer, 1);
    begin_command_buffer(command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL);

    return command_buffer;
}

void end_single_use_command_buffer(
    VkCommandBuffer command_buffer) {
    end_command_buffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(g_ctx->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_ctx->graphics_queue);

    vkFreeCommandBuffers(g_ctx->device, g_ctx->graphics_command_pool, 1, &command_buffer);
}

void submit_secondary_command_buffer(
    VkCommandBuffer pcommand_buffer,
    VkCommandBuffer scommand_buffer) {
    vkCmdExecuteCommands(
        pcommand_buffer,
        1, &scommand_buffer);
}

}
