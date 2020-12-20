#pragma once

#include <vulkan/vulkan.h>

namespace vk {

void init_command_pool();
void init_primary_command_buffers();

void create_command_buffers(VkCommandBufferLevel level, VkCommandBuffer *cmdbufs, uint32_t count);

void begin_command_buffer(
    VkCommandBuffer command_buffer,
    VkCommandBufferUsageFlags usage,
    VkCommandBufferInheritanceInfo *inheritance);
void end_command_buffer(VkCommandBuffer command_buffer);

// Some utility stuff
VkCommandBuffer begin_single_use_command_buffer();
void end_single_use_command_buffer(VkCommandBuffer command_buffer);

void submit_secondary_command_buffer(VkCommandBuffer pcommand_buffer, VkCommandBuffer scommand_buffer);

}
