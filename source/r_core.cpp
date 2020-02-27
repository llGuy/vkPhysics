#include "renderer.hpp"
#include "r_internal.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "imgui.h"
#include <vulkan/vulkan.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

static VkInstance instance;
static int32_t enable_validation_layers;
static uint32_t enabled_layer_count = 1;
static const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

// Check if requested layers exist, and puts all requested layers that exist into layers_dst array
static void s_verify_layer_support(
    const char **layers, uint32_t count, 
    const char **layers_dst, uint32_t *count_dst) {
    uint32_t supported_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&supported_layer_count, NULL);

    VkLayerProperties *supported_layers = ALLOCA(VkLayerProperties, supported_layer_count);
    vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_layers);

    uint32_t requested_verified_count = 0;

    for (uint32_t requested = 0; requested < count; ++requested) {
        int32_t found = 0;

        for (uint32_t supported = 0; supported < supported_layer_count; ++supported) {
            if (!strcmp(supported_layers[supported].layerName, layers[requested])) {
                layers_dst[requested_verified_count++] = layers[requested];
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Validation layer %s not supported\n", layers[requested]);
        }
    }

    *count_dst = requested_verified_count;
}

static void s_instance_init(const char *application_name) {
#ifdef NDEBUG
    int32_t enable_validation_layers = 0;

    uint32_t enabled_layer_count = 0;
    const char **validation_layers = nullptr;
#else
    enable_validation_layers = 1;

    const char *requested_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    s_verify_layer_support(requested_validation_layers, enabled_layer_count, validation_layers, &enabled_layer_count);
#endif

    uint32_t extension_count = 4;
    const char *extensions[] = {
        "VK_KHR_win32_surface",
        "VK_KHR_surface",
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report"
    };

    VkApplicationInfo application_info = {};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = NULL;
    application_info.pApplicationName = application_name;
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &application_info;
    instance_info.enabledLayerCount = enabled_layer_count;
    instance_info.ppEnabledLayerNames = validation_layers;
    instance_info.enabledExtensionCount = extension_count;
    instance_info.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        printf("Failed to create vulkan instance\n");
        exit(1);
    }
}

#ifndef NDEBUG
static VkDebugUtilsMessengerEXT debug_messenger;

static VKAPI_ATTR VkBool32 VKAPI_PTR s_debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    printf("Validation layer: %s\n", callback_data->pMessage);
    return 0;
}
#endif

static void s_debug_messenger_init() {
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
    debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.pNext = NULL;
    debug_messenger_info.flags = 0;
    debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = &s_debug_messenger_callback;
    debug_messenger_info.pUserData = NULL;

    PFN_vkCreateDebugUtilsMessengerEXT ptr_vkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (ptr_vkCreateDebugUtilsMessenger(instance, &debug_messenger_info, NULL, &debug_messenger)) {
        printf("Failed to create debug utils messenger\n");
    }
#endif
}

static VkSurfaceKHR surface;

typedef struct {
    int32_t graphics_family;
    int32_t present_family;
} queue_families_t;

static queue_families_t queue_families;

static int32_t s_queue_families_complete(queue_families_t *families) {
    return families->graphics_family >= 0 && families->present_family >= 0;
}

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t available_formats_count;
    VkSurfaceFormatKHR *available_formats;
    uint32_t available_present_modes_count;
    VkPresentModeKHR *available_present_modes;
} swapchain_details_t;

static swapchain_details_t swapchain_support;

static VkPhysicalDeviceMemoryProperties hardware_memory_properties;
static VkPhysicalDeviceProperties hardware_properties;
static VkFormat suitable_hardware_depth_format;
static VkPhysicalDevice hardware;
static VkDevice device;

static int32_t s_verify_hardware_meets_requirements(const char **extensions, uint32_t extension_count) {
    // Get queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(hardware, &queue_family_count, NULL);

    VkQueueFamilyProperties *queue_properties = ALLOCA(VkQueueFamilyProperties, queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(hardware, &queue_family_count, queue_properties);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_properties[i].queueCount > 0) {
            queue_families.graphics_family = i;
        }

        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(hardware, i, surface, &present_support);

        if (queue_properties[i].queueCount > 0 && present_support) {
            queue_families.present_family = i;
        }
        if (s_queue_families_complete(&queue_families)) {
            break;
        }
    }

    uint32_t available_extension_count;
    vkEnumerateDeviceExtensionProperties(hardware, NULL, &available_extension_count, NULL);

    VkExtensionProperties *extension_properties = ALLOCA(VkExtensionProperties, available_extension_count);
    vkEnumerateDeviceExtensionProperties(hardware, NULL, &available_extension_count, extension_properties);

    uint32_t required_extensions_left = extension_count;
    for (uint32_t i = 0; i < available_extension_count && required_extensions_left > 0; ++i) {
        for (uint32_t j = 0; j < extension_count; ++j) {
            if (!strcmp(extension_properties[i].extensionName, extensions[j])) {
                --required_extensions_left;
            }
        }
    }
    
    int32_t swapchain_supported = (!required_extensions_left);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(hardware, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(hardware, &device_features);

    int32_t swapchain_usable = 0;
    if (swapchain_supported) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(hardware, surface, &swapchain_support.capabilities);
        vkGetPhysicalDeviceSurfaceFormatsKHR(hardware, surface, &swapchain_support.available_formats_count, NULL);

        if (swapchain_support.available_formats_count != 0) {
            swapchain_support.available_formats = FL_MALLOC(VkSurfaceFormatKHR, swapchain_support.available_formats_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(hardware, surface, &swapchain_support.available_formats_count, swapchain_support.available_formats);
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(hardware, surface, &swapchain_support.available_present_modes_count, NULL);
        if (swapchain_support.available_present_modes_count != 0) {
            swapchain_support.available_present_modes = FL_MALLOC(VkPresentModeKHR, swapchain_support.available_present_modes_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(hardware, surface, &swapchain_support.available_present_modes_count, swapchain_support.available_present_modes);
        }

        swapchain_usable = swapchain_support.available_formats_count && swapchain_support.available_present_modes_count;
    }

    return swapchain_supported && swapchain_usable
        && device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && s_queue_families_complete(&queue_families)
        && device_features.geometryShader
        && device_features.wideLines
        && device_features.textureCompressionBC
        && device_features.samplerAnisotropy
        && device_features.fillModeNonSolid;
}

static VkFormat s_find_suitable_depth_format(VkFormat *formats, uint32_t format_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (uint32_t i = 0; i < format_count; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(hardware, formats[i], &properties);
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
            return(formats[i]);
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
            return(formats[i]);
        }
    }

    assert(0);

    return VkFormat(0);
}

static uint32_t s_pop_count(uint32_t bits) {
#ifndef __GNUC__
    return __popcnt(bits);
#else
    return __builtin_popcount(bits);
#endif
}

static VkQueue graphics_queue;
static VkQueue present_queue;

static void s_device_init() {
    uint32_t extension_count = 1;
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    VkPhysicalDevice *devices = ALLOCA(VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    for (uint32_t i = 0; i < device_count; ++i) {
        hardware = devices[i];

        if (s_verify_hardware_meets_requirements(extensions, extension_count)) {
            vkGetPhysicalDeviceProperties(hardware, &hardware_properties);
            break;
        }
    }

    assert(hardware != VK_NULL_HANDLE);

    vkGetPhysicalDeviceMemoryProperties(hardware, &hardware_memory_properties);

    VkFormat formats[] = {
        VK_FORMAT_D32_SFLOAT, 
        VK_FORMAT_D32_SFLOAT_S8_UINT, 
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    suitable_hardware_depth_format = s_find_suitable_depth_format(formats, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    uint32_t unique_queue_family_finder = 0;
    unique_queue_family_finder |= 1 << queue_families.graphics_family;
    unique_queue_family_finder |= 1 << queue_families.present_family;
    uint32_t unique_queue_family_count = s_pop_count(unique_queue_family_finder);

    uint32_t *unique_family_indices = ALLOCA(uint32_t, unique_queue_family_count);
    VkDeviceQueueCreateInfo *unique_family_infos = ALLOCA(VkDeviceQueueCreateInfo, unique_queue_family_count);

    for (uint32_t bit = 0, set_bits = 0; bit < 32 && set_bits < unique_queue_family_count; ++bit) {
        if (unique_queue_family_finder & (1 << bit)) {
            unique_family_indices[set_bits++] = bit;
        }
    }

    float priority1 = 1.0f;
    for (uint32_t i = 0; i < unique_queue_family_count; ++i) {
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.pNext = NULL;
        queue_info.flags = 0;
        queue_info.queueFamilyIndex = unique_family_indices[i];
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority1;
        
        unique_family_infos[i] = queue_info;
    }

    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.wideLines = VK_TRUE;
    device_features.geometryShader = VK_TRUE;
    device_features.fillModeNonSolid = VK_TRUE;

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = NULL;
    device_info.flags = 0;
    device_info.queueCreateInfoCount = unique_queue_family_count;
    device_info.pQueueCreateInfos = unique_family_infos;
    device_info.enabledLayerCount = enabled_layer_count;
    device_info.ppEnabledLayerNames = validation_layers;
    device_info.enabledExtensionCount = extension_count;
    device_info.ppEnabledExtensionNames = extensions;
    device_info.pEnabledFeatures = &device_features;

    if (vkCreateDevice(hardware, &device_info, NULL, &device) != VK_SUCCESS) {
        printf("Failed to create device\n");
        exit(1);
    }

    vkGetDeviceQueue(device, queue_families.graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_families.present_family, 0, &present_queue);
}

static VkSurfaceFormatKHR s_choose_surface_format(VkSurfaceFormatKHR *available_formats, uint32_t format_count) {
    if (format_count == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format;
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    for (uint32_t i = 0; i < format_count; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return(available_formats[i]);
        }
    }

    return(available_formats[0]);
}

static VkExtent2D s_choose_swapchain_extent(uint32_t width, uint32_t height, const VkSurfaceCapabilitiesKHR *capabilities) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return(capabilities->currentExtent);
    }
    else {
        VkExtent2D actual_extent = { (uint32_t)width, (uint32_t)height };
        actual_extent.width = max(capabilities->minImageExtent.width, min(capabilities->maxImageExtent.width, actual_extent.width));
        actual_extent.height = max(capabilities->minImageExtent.height, min(capabilities->maxImageExtent.height, actual_extent.height));

        return(actual_extent);
    }
}

static VkPresentModeKHR s_choose_surface_present_mode(const VkPresentModeKHR *available_present_modes, uint32_t present_mode_count) {
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return(available_present_modes[i]);
        }
        else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            best_mode = available_present_modes[i];
        }
    }
    return(best_mode);
}

typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkFormat format;
    VkExtent2D extent;
    VkPresentModeKHR present_mode;
} swapchain_t;

static swapchain_t swapchain;

static void s_swapchain_init(uint32_t window_width, uint32_t window_height) {
    swapchain_details_t *swapchain_details = &swapchain_support;
    VkSurfaceFormatKHR surface_format = s_choose_surface_format(swapchain_details->available_formats, swapchain_details->available_formats_count);
    VkExtent2D surface_extent = s_choose_swapchain_extent(window_width, window_height, &swapchain_details->capabilities);
    VkPresentModeKHR present_mode = s_choose_surface_present_mode(swapchain_details->available_present_modes, swapchain_details->available_present_modes_count);

    // add 1 to the minimum images supported in the swapchain
    uint32_t image_count = swapchain_details->capabilities.minImageCount + 1;
    if (image_count > swapchain_details->capabilities.maxImageCount) {
        image_count = swapchain_details->capabilities.maxImageCount;
    }

    uint32_t queue_family_indices[] = { (uint32_t)queue_families.graphics_family, (uint32_t)queue_families.present_family };

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = surface_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = (queue_families.graphics_family == queue_families.present_family) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = (queue_families.graphics_family == queue_families.present_family) ? 0 : 2;
    swapchain_info.pQueueFamilyIndices = (queue_families.graphics_family == queue_families.present_family) ? NULL : queue_family_indices;
    swapchain_info.preTransform = swapchain_details->capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &swapchain_info, NULL, &swapchain.swapchain) != VK_SUCCESS) {
        printf("Failed to create swapchain\n");
        exit(1);
    }

    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, NULL);

    swapchain.image_count = image_count;
    swapchain.images = FL_MALLOC(VkImage, swapchain.image_count);

    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, swapchain.images);

    swapchain.extent = surface_extent;
    swapchain.format = surface_format.format;
    swapchain.present_mode = present_mode;

    swapchain.image_views = FL_MALLOC(VkImageView, swapchain.image_count);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.image = swapchain.images[i];
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = swapchain.format;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &image_view_info, NULL, &swapchain.image_views[i]) != VK_SUCCESS) {
            printf("Failed to create swapchain image view\n");
            exit(1);
        }
    }
}

static VkCommandPool graphics_command_pool;

static void s_command_pool_init() {
    VkCommandPoolCreateInfo command_pool_info = {};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = queue_families.graphics_family;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &command_pool_info, NULL, &graphics_command_pool) != VK_SUCCESS) {
        printf("Failed to create command pool\n");
        exit(1);
    }
}

static uint32_t primary_command_buffer_count;
static VkCommandBuffer *primary_command_buffers;

static void s_primary_command_buffers_init() {
    primary_command_buffer_count = swapchain.image_count;
    primary_command_buffers = FL_MALLOC(VkCommandBuffer, primary_command_buffer_count);
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, primary_command_buffers, primary_command_buffer_count);
}

#define FRAMES_IN_FLIGHT 2
static VkSemaphore image_ready_semaphores[FRAMES_IN_FLIGHT];
static VkSemaphore render_finished_semaphores[FRAMES_IN_FLIGHT];
static VkFence fences[FRAMES_IN_FLIGHT];

static void s_synchronisation_init() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphore_info, NULL, &image_ready_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphore_info, NULL, &render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fence_info, NULL, &fences[i]) != VK_SUCCESS) {
            printf("Failed to create synchronisation primitives\n");
            exit(1);
        }
    }
}

static uint32_t final_framebuffer_count;
static VkFramebuffer *final_framebuffers;
static VkRenderPass final_render_pass;

static void s_final_render_pass_init() {
    VkAttachmentDescription render_pass_attachment = {};
    render_pass_attachment.format = swapchain.format;
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

    if (vkCreateRenderPass(device, &render_pass_info, NULL, &final_render_pass) != VK_SUCCESS) {
        printf("Failed to create final render pass\n");
        exit(1);
    }

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = final_render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.width = swapchain.extent.width;
    framebuffer_info.height = swapchain.extent.height;
    framebuffer_info.layers = 1;

    final_framebuffer_count = swapchain.image_count;
    final_framebuffers = FL_MALLOC(VkFramebuffer, final_framebuffer_count);

    for (uint32_t i = 0; i < final_framebuffer_count; ++i) {
        framebuffer_info.pAttachments = &swapchain.image_views[i];
        if (vkCreateFramebuffer(device, &framebuffer_info, NULL, &final_framebuffers[i]) != VK_SUCCESS) {
            printf("Failed to create swapchain framebuffers\n");
            exit(1);
        }
    }
}

static VkDescriptorPool descriptor_pool;

static void s_descriptor_pool_init() {
    VkDescriptorPoolSize sizes[11];
    sizes[0].descriptorCount = 1000; sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    sizes[1].descriptorCount = 1000; sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[2].descriptorCount = 1000; sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sizes[3].descriptorCount = 1000; sizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[4].descriptorCount = 1000; sizes[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    sizes[5].descriptorCount = 1000; sizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    sizes[6].descriptorCount = 1000; sizes[6].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[7].descriptorCount = 1000; sizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[8].descriptorCount = 1000; sizes[8].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    sizes[9].descriptorCount = 1000; sizes[9].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    sizes[10].descriptorCount = 1000; sizes[10].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes = sizes;

    if (vkCreateDescriptorPool(device, &pool_info, NULL, &descriptor_pool) != VK_SUCCESS) {
        printf("Failed to create descriptor pool\n");
        exit(1);
    }
}

static void s_imgui_callback(VkResult result) {
    
}

static VkRenderPass imgui_render_pass;

static imgui_proc_t imgui_proc;


static void s_imgui_init(void *vwindow, imgui_proc_t proc) {
    imgui_proc = proc;

    GLFWwindow *window = (GLFWwindow *)vwindow;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    VkAttachmentDescription attachment = {};
    attachment.format = swapchain.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &info, nullptr, &imgui_render_pass) != VK_SUCCESS) {
        printf("Failed to create render pass\n");
        exit(1);
    }

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = hardware;
    init_info.Device = device;
    init_info.QueueFamily = queue_families.graphics_family;
    init_info.Queue = graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool;
    init_info.Allocator = NULL;
    init_info.MinImageCount = swapchain.image_count;
    init_info.ImageCount = swapchain.image_count;
    init_info.CheckVkResultFn = &s_imgui_callback;
    ImGui_ImplVulkan_Init(&init_info, imgui_render_pass);

    ImGui::StyleColorsDark();

    VkCommandBuffer command_buffer = begin_simgle_time_command_buffer();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_single_time_command_buffer(command_buffer);
}

typedef struct {
    VkDescriptorSetLayout sampler;
    VkDescriptorSetLayout input_attachment;
    VkDescriptorSetLayout uniform_buffer;
} descriptor_layouts_t;

static descriptor_layouts_t descriptor_layouts;

static void s_global_descriptor_layouts_init() {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.sampler) != VK_SUCCESS) {
        printf("Failed to create descriptor set layout\n");
        exit(1);
    }

    binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    
    if (vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.input_attachment) != VK_SUCCESS) {
	printf("Failed to create descriptor set layout\n");
	exit(1);
    }

    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    if (vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.uniform_buffer) != VK_SUCCESS) {
	printf("Failed to create descriptor set layout\n");
	exit(1);
    }
}

void renderer_init(
    const char *application_name,
    surface_proc_t create_surface,
    imgui_proc_t debug_proc,
    void *window, 
    uint32_t window_width, 
    uint32_t window_height) {
    s_instance_init(application_name);
    s_debug_messenger_init();
    create_surface(instance, &surface, window);
    s_device_init();
    s_swapchain_init(window_width, window_height);
    s_command_pool_init();
    s_primary_command_buffers_init();
    s_synchronisation_init();
    s_descriptor_pool_init();
    s_final_render_pass_init();
    s_global_descriptor_layouts_init();
    r_pipeline_init();
    //s_imgui_init(window, debug_proc);
    
}

static uint32_t current_frame = 0;
static uint32_t image_index = 0;

void resize_swapchain() {
    /* ImGui stuff */
    ImGui_ImplVulkan_SetMinImageCount(swapchain.image_count);

    /*ImGui_ImplVulkanH_CreateWindow(instance, hardare, device, &g_MainWindowData,
    g_QueueFamily, g_Allocator, g_SwapChainResizeWidth, g_SwapChainResizeHeight, g_MinImageCount);*/
}

VkCommandBuffer begin_frame() {
    current_frame = 0;

    VkFence null_fence = VK_NULL_HANDLE;

    VkResult result = vkAcquireNextImageKHR(
        device, 
        swapchain.swapchain, 
        UINT64_MAX, 
        image_ready_semaphores[current_frame], 
        null_fence, 
        &image_index);

    vkWaitForFences(device, 1, &fences[current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fences[current_frame]);

    begin_command_buffer(primary_command_buffers[current_frame], 0, NULL);

    return primary_command_buffers[current_frame];
}

void end_frame() {
    VkOffset2D offset;
    memset(&offset, 0, sizeof(offset));

    VkRect2D render_area = {};
    render_area.offset = offset;
    render_area.extent = swapchain.extent;

    VkClearValue clear_value;
    memset(&clear_value, 0, sizeof(VkClearValue));

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = final_render_pass;
    render_pass_begin_info.framebuffer = final_framebuffers[image_index];
    render_pass_begin_info.renderArea = render_area;
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(primary_command_buffers[current_frame], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    r_execute_final_pass(primary_command_buffers[current_frame]);

/*    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    imgui_proc();

    ImGui::Render();

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), primary_command_buffers[current_frame]);   */

    vkCmdEndRenderPass(primary_command_buffers[current_frame]);



    end_command_buffer(primary_command_buffers[current_frame]);

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &primary_command_buffers[current_frame];
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_ready_semaphores[current_frame];
    submit_info.pWaitDstStageMask = &wait_stages;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphores[current_frame];
 
    vkQueueSubmit(graphics_queue, 1, &submit_info, fences[current_frame]);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores[current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain.swapchain;
    present_info.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);

    current_frame = (current_frame + 1) % FRAMES_IN_FLIGHT;
}

void create_command_buffers(
    VkCommandBufferLevel level,
    VkCommandBuffer *command_buffers, uint32_t count) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.level = level;
    allocate_info.commandPool = graphics_command_pool;
    allocate_info.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device, &allocate_info, command_buffers) != VK_SUCCESS) {
        printf("Failed to allocate command buffers\n");
        exit(1);
    }
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

VkCommandBuffer begin_simgle_time_command_buffer() {
    VkCommandBuffer command_buffer;
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, &command_buffer, 1);

    begin_command_buffer(command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL);
    return command_buffer;
}

void end_single_time_command_buffer(
    VkCommandBuffer command_buffer) {
    end_command_buffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device, graphics_command_pool, 1, &command_buffer);
}

void r_get_swapchain_images(uint32_t *image_count, attachment_t *attachments) {
    *image_count = swapchain.image_count;

    if (attachments) {
        for (uint32_t i = 0; i < swapchain.image_count; ++i) {
            attachments[i].image = swapchain.images[i];
            attachments[i].image_view = swapchain.image_views[i];
        }
    }
}

uint32_t r_get_image_index() {
    return image_index;
}

VkDevice r_device() {
    return device;
}

VkExtent2D r_swapchain_extent() {
    return swapchain.extent;
}

static uint32_t s_find_memory_type_according_to_requirements(
    VkMemoryPropertyFlags properties, 
    VkMemoryRequirements memory_requirements) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(hardware, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (memory_requirements.memoryTypeBits & (1 << i)
            && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return(i);
        }
    }

    assert(false);

    return(0);
}

VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties) {
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = s_find_memory_type_according_to_requirements(properties, requirements);

    VkDeviceMemory memory;
    vkAllocateMemory(device, &alloc_info, nullptr, &memory);

    vkBindImageMemory(device, image, memory, 0);

    return memory;
}

VkFormat r_swapchain_format() {
    return swapchain.format;
}

VkFormat r_depth_format() {
    return suitable_hardware_depth_format;
}

VkDescriptorSetLayout r_descriptor_layout(VkDescriptorType type) {
    switch (type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
        return descriptor_layouts.sampler;
    }
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
        return descriptor_layouts.input_attachment;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
        return descriptor_layouts.uniform_buffer;
    }
    default:
        printf("Specified invalid descriptor type\n");
        return (VkDescriptorSetLayout)0;
    }
}

VkDescriptorPool r_descriptor_pool() {
    return descriptor_pool;
}

VkRenderPass r_final_render_pass() {
    return final_render_pass;
}
