#pragma once

#include <vulkan/vulkan.h>

namespace vk {

struct texture_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkSampler sampler;
    VkFormat format;

    VkDescriptorSet descriptor;

    void init(
        const char *path,
        VkFormat format,
        void *data,
        uint32_t width, uint32_t height,
        VkFilter filter);
};

}
