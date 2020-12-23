#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace vk {

constexpr uint32_t MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE = 5;

struct descriptor_layouts_t {
    VkDescriptorSetLayout sampler[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
    VkDescriptorSetLayout input_attachment[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
    VkDescriptorSetLayout uniform_buffer[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
};

void init_descriptor_pool();
void init_descriptor_layouts();

/*
 Handy little function that given a type and count, will create a descriptor layout
 if it doesn't exist yet. They are all stored in the descriptor_layouts struct in g_ctx
*/
VkDescriptorSetLayout get_descriptor_layout(VkDescriptorType type, uint32_t count);

// Creates a descriptor set with one binding (one image)
VkDescriptorSet create_image_descriptor_set(VkImageView image, VkSampler sampler, VkDescriptorType type);
// Overload creates a descriptor with multiple bindings (multiple images)
VkDescriptorSet create_image_descriptor_set(VkImageView *images, VkSampler *samplers, uint32_t count, VkDescriptorType type);
}
