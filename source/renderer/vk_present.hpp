#pragma once

#include <vk.hpp>
#include <vulkan/vulkan.h>

namespace app {

struct context_info_t;

}

namespace vk {

struct swapchain_support_t {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t available_formats_count;
    VkSurfaceFormatKHR *available_formats;
    uint32_t available_present_modes_count;
    VkPresentModeKHR *available_present_modes;
};

struct swapchain_t {
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkFormat format;
    VkExtent2D extent;
    VkPresentModeKHR present_mode;

    uint32_t final_fbo_count;
    VkFramebuffer *final_fbos;
    VkRenderPass final_pass;
};

void init_surface(const app::context_info_t *app_ctx);
void init_swapchain(uint32_t width, uint32_t height);

void resize_swapchain(uint32_t width, uint32_t height);

// Fills in attachment_t structs with images of swapchain
void get_swapchain_images(uint32_t *image_count, struct attachment_t *attachments);

}
