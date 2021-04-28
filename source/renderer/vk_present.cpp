#include "vk_present.hpp"
#include "vk_context.hpp"
#include "app_context.hpp"
#include "vk_render_pipeline.hpp"

#include <tools.hpp>
#include <allocators.hpp>

namespace vk {

void init_surface(const app::context_info_t *app_ctx) {
    app_ctx->surface_proc(g_ctx->instance, &g_ctx->surface, app_ctx->window);
}

static VkSurfaceFormatKHR s_choose_surface_format(
    VkSurfaceFormatKHR *available_formats,
    uint32_t format_count) {
    if (format_count == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format;
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    for (uint32_t i = 0; i < format_count; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return(available_formats[i]);
        }
    }

    return available_formats[0];
}

static VkExtent2D s_choose_swapchain_extent(
    uint32_t width,
    uint32_t height,
    const VkSurfaceCapabilitiesKHR *capabilities) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return(capabilities->currentExtent);
    }
    else {
        VkExtent2D actual_extent = { (uint32_t)width, (uint32_t)height };
        actual_extent.width = MAX(
            capabilities->minImageExtent.width,
            MIN(
                capabilities->maxImageExtent.width,
                actual_extent.width));

        actual_extent.height = MAX(
            capabilities->minImageExtent.height,
            MIN(
                capabilities->maxImageExtent.height,
                actual_extent.height));

        return actual_extent;
    }
}

static VkPresentModeKHR s_choose_surface_present_mode(
    const VkPresentModeKHR *available_present_modes,
    uint32_t present_mode_count) {
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return(available_present_modes[i]);
        }
        else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            best_mode = available_present_modes[i];
        }
    }
    return best_mode;
}

static void s_init_swapchain_fbos() {
    VkAttachmentDescription render_pass_attachment = {};
    render_pass_attachment.format = g_ctx->swapchain.format;
    render_pass_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    render_pass_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    render_pass_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    render_pass_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    render_pass_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference = {};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkSubpassDependency subpass_dependency = {};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &render_pass_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &subpass_dependency;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &g_ctx->swapchain.final_pass));

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = g_ctx->swapchain.final_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.width = g_ctx->swapchain.extent.width;
    framebuffer_info.height = g_ctx->swapchain.extent.height;
    framebuffer_info.layers = 1;

    g_ctx->swapchain.final_fbo_count = g_ctx->swapchain.image_count;
    g_ctx->swapchain.final_fbos = flmalloc<VkFramebuffer>(g_ctx->swapchain.final_fbo_count);

    for (uint32_t i = 0; i < g_ctx->swapchain.final_fbo_count; ++i) {
        framebuffer_info.pAttachments = &g_ctx->swapchain.image_views[i];
        VK_CHECK(vkCreateFramebuffer(g_ctx->device, &framebuffer_info, NULL, &g_ctx->swapchain.final_fbos[i]));
    }
}

void init_swapchain(uint32_t width, uint32_t height) {
    swapchain_support_t *swapchain_details = &g_ctx->swapchain_support;

    VkSurfaceFormatKHR surface_format = s_choose_surface_format(
        swapchain_details->available_formats,
        swapchain_details->available_formats_count);

    VkExtent2D surface_extent = s_choose_swapchain_extent(
        width,
        height,
        &swapchain_details->capabilities);

    VkPresentModeKHR present_mode = s_choose_surface_present_mode(
        swapchain_details->available_present_modes,
        swapchain_details->available_present_modes_count);

    // add 1 to the minimum images supported in the swapchain
    uint32_t image_count = swapchain_details->capabilities.minImageCount + 1;

    if (image_count > swapchain_details->capabilities.maxImageCount &&
        swapchain_details->capabilities.maxImageCount)
        image_count = swapchain_details->capabilities.maxImageCount;

    uint32_t queue_family_indices[] = {
        (uint32_t)g_ctx->queue_families.graphics_family,
        (uint32_t)g_ctx->queue_families.present_family
    };

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = g_ctx->surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = surface_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode =
        (g_ctx->queue_families.graphics_family == g_ctx->queue_families.present_family) ?
        VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount =
        (g_ctx->queue_families.graphics_family == g_ctx->queue_families.present_family) ?
        0 : 2;
    swapchain_info.pQueueFamilyIndices =
        (g_ctx->queue_families.graphics_family == g_ctx->queue_families.present_family) ?
        NULL : queue_family_indices;
    swapchain_info.preTransform = swapchain_details->capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(g_ctx->device, &swapchain_info, NULL, &g_ctx->swapchain.swapchain));

    vkGetSwapchainImagesKHR(g_ctx->device, g_ctx->swapchain.swapchain, &image_count, NULL);

    g_ctx->swapchain.image_count = image_count;
    g_ctx->swapchain.images = flmalloc<VkImage>(g_ctx->swapchain.image_count);

    vkGetSwapchainImagesKHR(g_ctx->device, g_ctx->swapchain.swapchain, &image_count, g_ctx->swapchain.images);

    g_ctx->swapchain.extent = surface_extent;
    g_ctx->swapchain.format = surface_format.format;
    g_ctx->swapchain.present_mode = present_mode;

    g_ctx->swapchain.image_views = flmalloc<VkImageView>(g_ctx->swapchain.image_count);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.image = g_ctx->swapchain.images[i];
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = g_ctx->swapchain.format;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(g_ctx->device, &image_view_info, NULL, &g_ctx->swapchain.image_views[i]));
    }

    s_init_swapchain_fbos();
}

void resize_swapchain(
    uint32_t width,
    uint32_t height) {
    vkDestroySwapchainKHR(g_ctx->device, g_ctx->swapchain.swapchain, NULL);

    for (uint32_t i = 0; i < g_ctx->swapchain.image_count; ++i) {
        vkDestroyImageView(g_ctx->device, g_ctx->swapchain.image_views[i], NULL);
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx->hardware, g_ctx->surface, &g_ctx->swapchain_support.capabilities);

    vkDestroyRenderPass(g_ctx->device, g_ctx->swapchain.final_pass, NULL);
    for (uint32_t i = 0; i < g_ctx->swapchain.final_fbo_count; ++i) {
        vkDestroyFramebuffer(g_ctx->device, g_ctx->swapchain.final_fbos[i], NULL);
    }

    init_swapchain(width, height);
}

swapchain_information_t get_swapchain_info() {
    swapchain_information_t dst = {};
    dst.frames_in_flight = FRAMES_IN_FLIGHT;
    dst.image_count = g_ctx->swapchain.image_count;
    dst.width = g_ctx->swapchain.extent.width;
    dst.height = g_ctx->swapchain.extent.height;

    return dst;
}

void get_swapchain_images(uint32_t *image_count, attachment_t *attachments) {
    *image_count = g_ctx->swapchain.image_count;

    if (attachments) {
        for (uint32_t i = 0; i < g_ctx->swapchain.image_count; ++i) {
            attachments[i].image = g_ctx->swapchain.images[i];
            attachments[i].image_view = g_ctx->swapchain.image_views[i];
        }
    }
}

}
