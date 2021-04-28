#pragma once

#include <vulkan/vulkan.h>

namespace vk {

void init_command_pool();
void init_primary_command_buffers();

// Some utility stuff
VkCommandBuffer begin_single_use_command_buffer();
void end_single_use_command_buffer(VkCommandBuffer command_buffer);

}
