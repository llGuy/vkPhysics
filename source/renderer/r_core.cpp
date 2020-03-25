// Renderer core

#include "renderer.hpp"
#include "r_internal.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <imgui.h>
#include <stb_image.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
#ifdef _WIN32
        "VK_KHR_win32_surface",
#else
        "VK_KHR_xcb_surface",
#endif
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

    VK_CHECK(vkCreateInstance(&instance_info, NULL, &instance));
}

#ifndef NDEBUG
static VkDebugUtilsMessengerEXT debug_messenger;

static VKAPI_ATTR VkBool32 VKAPI_PTR s_debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *) {
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

struct queue_families_t {
    int32_t graphics_family;
    int32_t present_family;
};

static queue_families_t queue_families;

static int32_t s_queue_families_complete(queue_families_t *families) {
    return families->graphics_family >= 0 && families->present_family >= 0;
}

struct swapchain_details_t {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t available_formats_count;
    VkSurfaceFormatKHR *available_formats;
    uint32_t available_present_modes_count;
    VkPresentModeKHR *available_present_modes;
};

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

    printf("Found %d devices\n", device_count);
    
    for (uint32_t i = 0; i < device_count; ++i) {
        hardware = devices[i];

        vkGetPhysicalDeviceProperties(hardware, &hardware_properties);
        
        printf("Device name: %s\n", hardware_properties.deviceName);
        
        if (s_verify_hardware_meets_requirements(extensions, extension_count)) {
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

    VK_CHECK(vkCreateDevice(hardware, &device_info, NULL, &device));

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
        actual_extent.width = MAX(capabilities->minImageExtent.width, MIN(capabilities->maxImageExtent.width, actual_extent.width));
        actual_extent.height = MAX(capabilities->minImageExtent.height, MIN(capabilities->maxImageExtent.height, actual_extent.height));

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

struct swapchain_t {
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkFormat format;
    VkExtent2D extent;
    VkPresentModeKHR present_mode;
};

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

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_info, NULL, &swapchain.swapchain));

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

        VK_CHECK(vkCreateImageView(device, &image_view_info, NULL, &swapchain.image_views[i]));
    }
}

static VkCommandPool graphics_command_pool;

static void s_command_pool_init() {
    VkCommandPoolCreateInfo command_pool_info = {};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = queue_families.graphics_family;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &command_pool_info, NULL, &graphics_command_pool));
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

    VK_CHECK(vkCreateRenderPass(device, &render_pass_info, NULL, &final_render_pass));

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
        VK_CHECK(vkCreateFramebuffer(device, &framebuffer_info, NULL, &final_framebuffers[i]));
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
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, NULL, &descriptor_pool));
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

    VK_CHECK(vkCreateRenderPass(device, &info, nullptr, &imgui_render_pass));

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

    VkCommandBuffer command_buffer = begin_single_time_command_buffer();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_single_time_command_buffer(command_buffer);
}

#define MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE 5

struct descriptor_layouts_t {
    VkDescriptorSetLayout sampler[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
    VkDescriptorSetLayout input_attachment[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
    VkDescriptorSetLayout uniform_buffer[MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE];
};

static descriptor_layouts_t descriptor_layouts;

static void s_global_descriptor_layouts_init() {
    memset(&descriptor_layouts, 0, sizeof(descriptor_layouts));
    
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.sampler[0]));

    binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.input_attachment[0]));

    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.uniform_buffer[0]));
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
    
    r_camera_init(window);
    r_lighting_init();
    r_pipeline_init();
    r_environment_init();
    
    s_imgui_init(window, debug_proc);
}

static uint32_t current_frame = 0;
static uint32_t image_index = 0;

void resize_swapchain(
    uint32_t width,
    uint32_t height) {
    vkDestroySwapchainKHR(r_device(), swapchain.swapchain, NULL);

    for (uint32_t i = 0; i < swapchain.image_count; ++i) {
        vkDestroyImageView(r_device(), swapchain.image_views[i], NULL);
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(hardware, surface, &swapchain_support.capabilities);

    s_swapchain_init(width, height);

    vkDestroyRenderPass(r_device(), final_render_pass, NULL);

    for (uint32_t i = 0; i < final_framebuffer_count; ++i) {
        vkDestroyFramebuffer(r_device(), final_framebuffers[i], NULL);
    }

    s_final_render_pass_init();
}

void handle_resize(
    uint32_t width,
    uint32_t height) {
    vkDeviceWaitIdle(r_device());

    resize_swapchain(width, height);
    r_handle_resize(width, height);
}

void swapchain_information(
    swapchain_information_t *dst) {
    dst->frames_in_flight = FRAMES_IN_FLIGHT;
    dst->image_count = swapchain.image_count;
    dst->width = swapchain.extent.width;
    dst->height = swapchain.extent.height;
}

VkCommandBuffer begin_frame() {
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

    begin_command_buffer(primary_command_buffers[image_index], 0, NULL);

    return primary_command_buffers[image_index];
}

void gpu_data_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye_info,
    lighting_info_t *lighting_info) {
    r_camera_gpu_sync(command_buffer, eye_info);
    r_lighting_gpu_sync(command_buffer, lighting_info);
    r_render_environment_to_offscreen_if_updated(command_buffer);
}

void end_frame() {
    VkOffset2D offset;
    memset(&offset, 0, sizeof(offset));

    r_execute_ssao_pass(primary_command_buffers[image_index]);
    r_execute_ssao_blur_pass(primary_command_buffers[image_index]);
    r_execute_lighting_pass(primary_command_buffers[image_index]);
    r_execute_motion_blur_pass(primary_command_buffers[image_index]);
    //r_execute_bloom_pass(primary_command_buffers[image_index]);

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
    
    vkCmdBeginRenderPass(primary_command_buffers[image_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    r_execute_final_pass(primary_command_buffers[image_index]);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // General stuff
    ImGui::Begin("General");
    ImGui::Text("Framerate: %.1f", ImGui::GetIO().Framerate);

    r_environment_debug_menu();
    
    ImGui::End();

    ImGui::Render();

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), primary_command_buffers[image_index]);

    vkCmdEndRenderPass(primary_command_buffers[image_index]);

    end_command_buffer(primary_command_buffers[image_index]);

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &primary_command_buffers[image_index];
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

    VK_CHECK(vkAllocateCommandBuffers(device, &allocate_info, command_buffers));
}

void fill_main_inheritance_info(
    VkCommandBufferInheritanceInfo *info) {
    rpipeline_stage_t *stage = r_deferred_stage();

    info->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    info->renderPass = stage->render_pass;
    info->subpass = 0;
    info->framebuffer = stage->framebuffer;
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

void submit_secondary_command_buffer(
    VkCommandBuffer pcommand_buffer,
    VkCommandBuffer scommand_buffer) {
    vkCmdExecuteCommands(
        pcommand_buffer,
        1, &scommand_buffer);
}

VkCommandBuffer begin_single_time_command_buffer() {
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

VkDeviceMemory allocate_gpu_buffer_memory(
    VkBuffer buffer,
    VkMemoryPropertyFlags properties) {
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = s_find_memory_type_according_to_requirements(properties, requirements);

    VkDeviceMemory memory;
    vkAllocateMemory(device, &alloc_info, nullptr, &memory);

    vkBindBufferMemory(device, buffer, memory, 0);

    return memory;
}

gpu_buffer_t create_gpu_buffer(
    uint32_t size,
    void *data,
    VkBufferUsageFlags usage) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = data ? usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT : usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(r_device(), &buffer_info, NULL, &buffer);

    VkDeviceMemory memory = allocate_gpu_buffer_memory(
        buffer,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    gpu_buffer_t gpu_buffer = {};
    gpu_buffer.buffer = buffer;
    gpu_buffer.memory = memory;
    gpu_buffer.usage = usage;
    gpu_buffer.size = (VkDeviceSize)size;

    if (data) {
        // Create staging buffer
        VkBufferCreateInfo staging_buffer_info = {};
        staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_info.size = size;
        staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer staging_buffer;
        vkCreateBuffer(r_device(), &staging_buffer_info, NULL, &staging_buffer);

        VkDeviceMemory staging_memory = allocate_gpu_buffer_memory(
            staging_buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void *staging_map;
        vkMapMemory(r_device(), staging_memory, 0, size, 0, &staging_map);

        memcpy(staging_map, data, size);

        /*VkMappedMemoryRange range = {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = staging_memory;
        range.offset = 0;
        range.size = size;
        vkFlushMappedMemoryRanges(r_device(), 1, &range);*/

        vkUnmapMemory(r_device(), staging_memory);
        
        VkCommandBuffer command_buffer = begin_single_time_command_buffer();
        
        VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            &gpu_buffer,
            0,
            size);

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
        vkCmdCopyBuffer(command_buffer, staging_buffer, gpu_buffer.buffer, 1, &copy_region);

        barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            &gpu_buffer,
            0,
            size);

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, NULL,
            1, &barrier,
            0, NULL);

        end_single_time_command_buffer(command_buffer);

        vkFreeMemory(r_device(), staging_memory, NULL);
        vkDestroyBuffer(r_device(), staging_buffer, NULL);
    }

    return gpu_buffer;
}

void update_gpu_buffer(
    VkCommandBuffer command_buffer,
    VkPipelineStageFlags pipeline_stage,
    uint32_t offset,
    uint32_t data_size,
    void *data,
    gpu_buffer_t *gpu_buffer) {
    VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
        pipeline_stage,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        gpu_buffer,
        offset,
        data_size);

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
        gpu_buffer->buffer,
        offset,
        data_size,
        data);

    barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        pipeline_stage,
        gpu_buffer,
        offset,
        data_size);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        pipeline_stage,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);
}

void destroy_gpu_buffer(
    gpu_buffer_t gpu_buffer) {
    vkFreeMemory(r_device(), gpu_buffer.memory, NULL);
    vkDestroyBuffer(r_device(), gpu_buffer.buffer, NULL);
}

static VkAccessFlags s_find_access_flags_for_stage(VkPipelineStageFlags stage) {
    switch (stage) {
    case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT: case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT: case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT: return VK_ACCESS_UNIFORM_READ_BIT;
    case VK_PIPELINE_STAGE_TRANSFER_BIT: return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    default: exit(1); return 0;
    }
}

static VkAccessFlags s_find_access_flags_for_image_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return 0;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    default: exit(1); return (VkAccessFlagBits)0;
    }
}

static VkPipelineStageFlags s_find_pipeline_stage_for_image_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    default: exit(1); return (VkPipelineStageFlags)0;
    }
}

VkImageMemoryBarrier create_image_barrier(
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkImage image,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_mip,
    uint32_t mip_levels,
    VkImageAspectFlags aspect) {
    VkImageMemoryBarrier image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = s_find_access_flags_for_image_layout(old_image_layout);
    image_barrier.dstAccessMask = s_find_access_flags_for_image_layout(new_image_layout);
    image_barrier.oldLayout = old_image_layout;
    image_barrier.newLayout = new_image_layout;
    image_barrier.image = image;
    image_barrier.subresourceRange.aspectMask = aspect;
    image_barrier.subresourceRange.baseMipLevel = base_mip;
    image_barrier.subresourceRange.levelCount = mip_levels;
    image_barrier.subresourceRange.baseArrayLayer = base_layer;
    image_barrier.subresourceRange.layerCount = layer_count;

    return image_barrier;
}

VkBufferMemoryBarrier create_gpu_buffer_barrier(
    VkPipelineStageFlags src,
    VkPipelineStageFlags dst,
    gpu_buffer_t *buffer,
    uint32_t offset,
    uint32_t max) {
    VkBufferMemoryBarrier buffer_barrier = {};
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.buffer = buffer->buffer;
    buffer_barrier.size = buffer->size;
    buffer_barrier.offset = offset;
    buffer_barrier.size = max;
    buffer_barrier.srcAccessMask = s_find_access_flags_for_stage(src);
    buffer_barrier.dstAccessMask = s_find_access_flags_for_stage(dst);

    return buffer_barrier;
}

VkFormat r_swapchain_format() {
    return swapchain.format;
}

VkFormat r_depth_format() {
    return suitable_hardware_depth_format;
}

VkDescriptorSetLayout r_descriptor_layout(
    VkDescriptorType type,
    uint32_t count) {
    if (count > MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE) {
        printf("Descriptor count for layout is too high\n");
        exit(1);
        return VK_NULL_HANDLE;
    }
    
    switch (type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
        if (descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);
            memset(bindings, 0, sizeof(VkDescriptorSetLayoutBinding) * count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.sampler[count - 1]));
        }

        return descriptor_layouts.sampler[count - 1];
    }
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
        if (descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.sampler[count - 1]));
        }
        
        return descriptor_layouts.input_attachment[count - 1];
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
        if (descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_layouts.sampler[count - 1]));
        }
        
        return descriptor_layouts.uniform_buffer[count - 1];
    }
    default:
        printf("Specified invalid descriptor type\n");
        return (VkDescriptorSetLayout)0;
    }
}

VkDescriptorSet create_image_descriptor_set(
    VkImageView image,
    VkSampler sampler,
    VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = r_descriptor_layout(type, 1);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = r_descriptor_pool();
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(r_device(), &allocate_info, &set);

    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image;
    image_info.sampler = sampler;
    
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(r_device(), 1, &write, 0, NULL);

    return set;
}

VkDescriptorSet create_image_descriptor_set(
    VkImageView *images,
    VkSampler *samplers,
    uint32_t count,
    VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = r_descriptor_layout(type, count);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = r_descriptor_pool();
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(r_device(), &allocate_info, &set);

    VkDescriptorImageInfo *image_info = ALLOCA(VkDescriptorImageInfo, count);
    VkWriteDescriptorSet *write = ALLOCA(VkWriteDescriptorSet, count);
    memset(image_info, 0, sizeof(VkDescriptorImageInfo) * count);
    memset(write, 0, sizeof(VkWriteDescriptorSet) * count);
    
    for (uint32_t i = 0; i < count; ++i) {
        image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[i].imageView = images[i];
        image_info[i].sampler = samplers[i];

        write[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[i].dstSet = set;
        write[i].dstBinding = i;
        write[i].dstArrayElement = 0;
        write[i].descriptorCount = 1;
        write[i].descriptorType = type;
        write[i].pImageInfo = &image_info[i];
    }

    vkUpdateDescriptorSets(r_device(), count, write, 0, NULL);

    return set;
}

VkDescriptorSet create_buffer_descriptor_set(
    VkBuffer buffer,
    VkDeviceSize buffer_size,
    VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = r_descriptor_layout(type, 1);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = r_descriptor_pool();
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(r_device(), &allocate_info, &set);

    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = buffer;
    buffer_info.offset = 0;
    buffer_info.range = buffer_size;
    
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(r_device(), 1, &write, 0, NULL);

    return set;
}

VkDescriptorPool r_descriptor_pool() {
    return descriptor_pool;
}

VkRenderPass r_final_render_pass() {
    return final_render_pass;
}

static char *s_read_shader(
    const char *path,
    uint32_t *file_size) {
    FILE *shader = fopen(path, "rb");
    fseek(shader, 0L, SEEK_END);
    *file_size = ftell(shader);
    char *code = (char *)malloc(sizeof(char) * (*file_size));
    rewind(shader);
    fread(code, sizeof(char), *file_size, shader);
    return code;
}

VkPipelineShaderStageCreateInfo *r_fill_shader_stage_create_infos(
    const char **paths,
    VkShaderStageFlags flags) {
    uint32_t count = s_pop_count(flags);

    VkShaderStageFlagBits bits_order[] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    VkPipelineShaderStageCreateInfo *infos = FL_MALLOC(VkPipelineShaderStageCreateInfo, count);
    memset(infos, 0, sizeof(VkPipelineShaderStageCreateInfo) * count);
    VkShaderModule *modules = FL_MALLOC(VkShaderModule, count);
    
    for (uint32_t bit = 0, current = 0; bit < sizeof(bits_order) / sizeof(VkShaderStageFlagBits); ++bit) {
        if (flags & bits_order[bit]) {
            uint32_t code_length;
            char *code = s_read_shader(paths[current], &code_length);

            VkShaderModuleCreateInfo shader_info = {};
            shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shader_info.codeSize = code_length;
            shader_info.pCode = (uint32_t *)code;
            vkCreateShaderModule(r_device(), &shader_info, NULL, &modules[current]);

            infos[current].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            infos[current].pName = "main";
            infos[current].stage = bits_order[bit];
            infos[current].module = modules[current];

            ++current;
        }
    }

    free(modules);

    return infos;
}

void r_free_shader_stage_create_info(
    VkPipelineShaderStageCreateInfo *info) {
    free(info);
}

attachment_t r_create_color_attachment(
    VkExtent3D extent, 
    VkFormat format) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    vkCreateImage(r_device(), &image_info, NULL, &image);

    VkDeviceMemory memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(r_device(), &image_view_info, NULL, &image_view));

    VkSampler sampler;
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(r_device(), &sampler_info, NULL, &sampler));

    attachment_t attachment;
    attachment.image = image;
    attachment.image_view = image_view;
    attachment.image_memory = memory;
    attachment.format = format;
    attachment.sampler = sampler;

    return attachment;
}

attachment_t r_create_depth_attachment(
    VkExtent3D extent) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = r_depth_format();
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    vkCreateImage(r_device(), &image_info, NULL, &image);

    VkDeviceMemory memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = r_depth_format();
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(r_device(), &image_view_info, NULL, &image_view));

    VkSampler sampler;
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(r_device(), &sampler_info, NULL, &sampler));

    attachment_t attachment;
    attachment.image = image;
    attachment.image_view = image_view;
    attachment.image_memory = memory;
    attachment.sampler = sampler;
    attachment.format = r_depth_format();

    return attachment;
}

VkFramebuffer r_create_framebuffer(
    uint32_t color_attachment_count,
    attachment_t *color_attachments, 
    attachment_t *depth_attachment,
    VkRenderPass render_pass,
    VkExtent2D extent,
    uint32_t output_layer_count) {
    uint32_t attachment_count = color_attachment_count + (depth_attachment ? 1 : 0);
    VkImageView *attachments = FL_MALLOC(VkImageView, attachment_count);
    for (uint32_t i = 0; i < color_attachment_count; ++i) {
        attachments[i] = color_attachments[i].image_view;
    }
    if (depth_attachment) {
        attachments[color_attachment_count] = depth_attachment->image_view;
    }

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.attachmentCount = attachment_count;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = output_layer_count;
    framebuffer_info.renderPass = render_pass;

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(r_device(), &framebuffer_info, NULL, &framebuffer);

    free(attachments);

    return framebuffer;
}

void r_rpipeline_descriptor_set_output_init(
    rpipeline_stage_t *stage) {
    VkImageView *views = ALLOCA(VkImageView, stage->color_attachment_count + 1);
    VkSampler *samplers = ALLOCA(VkSampler, stage->color_attachment_count + 1);
    
    uint32_t binding_count = 0;

    for (; binding_count < stage->color_attachment_count; ++binding_count) {
        views[binding_count] = stage->color_attachments[binding_count].image_view;
        samplers[binding_count] = stage->color_attachments[binding_count].sampler;
    }

    if (stage->depth_attachment) {
        views[binding_count] = stage->depth_attachment->image_view;
        samplers[binding_count] = stage->depth_attachment->sampler;
        
        ++binding_count;
    }

    stage->binding_count = binding_count;

    stage->descriptor_set = create_image_descriptor_set(
        views,
        samplers,
        binding_count,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

VkAttachmentDescription r_fill_color_attachment_description(VkImageLayout layout, VkFormat format) {
    VkAttachmentDescription description = {};
    description.finalLayout = layout;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    description.format = format;
    description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.samples = VK_SAMPLE_COUNT_1_BIT;

    return description;
}

VkAttachmentDescription r_fill_depth_attachment_description(VkImageLayout layout) {
    VkAttachmentDescription description = {};
    description.finalLayout = layout;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    description.format = r_depth_format();
    description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.samples = VK_SAMPLE_COUNT_1_BIT;

    return description;
}

texture_t create_texture(
    const char *path,
    VkFormat format,
    void *data,
    uint32_t width, uint32_t height,
    VkFilter filter) {
    uint32_t pixel_component_size = 1;
    int32_t x, y, channels;
    void *pixels;
    if (!data && path) {
        pixels = stbi_load(path, &x, &y, &channels, STBI_rgb_alpha);
    }
    else {
        switch (format) {
        case VK_FORMAT_R16G16B16A16_SFLOAT: {
            channels = 4;
            pixel_component_size = 2;
            break;
        }
        default: {
            printf("Need to handle format type for creating texture\n");
            exit(1);
            break;
        }
        }
        
        pixels = data;
        x = width;
        y = height;
    }

    VkExtent3D extent = {};
    extent.width = x;
    extent.height = y;
    extent.depth = 1;
    
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    vkCreateImage(r_device(), &image_info, NULL, &image);

    VkDeviceMemory memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(r_device(), &image_view_info, NULL, &image_view));

    VkSampler sampler;
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(r_device(), &sampler_info, NULL, &sampler));

    
    texture_t texture;
    texture.image = image;
    texture.image_view = image_view;
    texture.image_memory = memory;
    texture.sampler = sampler;
    texture.format = format;
    
    texture.descriptor = create_image_descriptor_set(image_view, sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    gpu_buffer_t staging = create_gpu_buffer(
        pixel_component_size * x * y * channels,
        pixels,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VkCommandBuffer command_buffer = begin_single_time_command_buffer();

    VkImageMemoryBarrier barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        image,
        0,
        1,
        0,
        1,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageExtent.width = x;
    region.imageExtent.height = y;
    region.imageExtent.depth = 1;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    
    vkCmdCopyBufferToImage(
        command_buffer,
        staging.buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        image,
        0,
        1,
        0,
        1,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    end_single_time_command_buffer(command_buffer);

    vkFreeMemory(r_device(), staging.memory, NULL);
    vkDestroyBuffer(r_device(), staging.buffer, NULL);

    return texture;
}

VkPipelineLayout r_create_pipeline_layout(
    VkShaderStageFlags shader_flags,
    VkDescriptorType *descriptor_types,
    uint32_t descriptor_layout_count,
    uint32_t push_constant_size) {
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = shader_flags;
    push_constant_range.offset = 0;
    push_constant_range.size = push_constant_size;

    VkDescriptorSetLayout *layouts = ALLOCA(VkDescriptorSetLayout, descriptor_layout_count);
    for (uint32_t i = 0; i < descriptor_layout_count; ++i) {
        layouts[i] = r_descriptor_layout(descriptor_types[i], 1);
    }
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = descriptor_layout_count;
    pipeline_layout_info.pSetLayouts = layouts;
    if (push_constant_size) {
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    }
    
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(r_device(), &pipeline_layout_info, NULL, &pipeline_layout);

    return pipeline_layout;
}

shader_t create_2d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    rpipeline_stage_t *stage,
    VkPrimitiveTopology topology) {
    VkPipelineLayout layout = r_create_pipeline_layout(
        shader_flags,
        descriptor_layout_types,
        descriptor_layout_count,
        push_constant_size);
    
    VkPipelineShaderStageCreateInfo *shader_infos = r_fill_shader_stage_create_infos(shader_paths, shader_flags);

    /* Is all zero for rendering pipeline shaders */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    if (binding_info) {
        vertex_input_info.vertexBindingDescriptionCount = binding_info->binding_count;
        vertex_input_info.pVertexBindingDescriptions = binding_info->binding_descriptions;
        vertex_input_info.vertexAttributeDescriptionCount = binding_info->attribute_count;
        vertex_input_info.pVertexAttributeDescriptions = binding_info->attribute_descriptions;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = topology;

    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1.0f;

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    
    VkPipelineViewportStateCreateInfo viewport_info {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &rect;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.minSampleShading = 1.0f;

    VkPipelineColorBlendStateCreateInfo blend_info = r_fill_blend_state_info(stage);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info {};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_info.depthTestEnable = VK_FALSE;
    depth_stencil_info.depthWriteEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = s_pop_count(shader_flags);
    pipeline_info.pStages = shader_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = &depth_stencil_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = stage->render_pass;
    /* For now just support one subpass per render pass */
    pipeline_info.subpass = 0;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(r_device(), VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    r_free_shader_stage_create_info(shader_infos);
    r_free_blend_state_info(&blend_info);

    shader_t shader = {};
    shader.pipeline = pipeline;
    shader.layout = layout;
    shader.flags = shader_flags;

    return shader;
}

static shader_t s_create_3d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    rpipeline_stage_t *stage,
    VkCullModeFlags cull_mode) {
    VkPipelineLayout layout = r_create_pipeline_layout(
        shader_flags,
        descriptor_layout_types,
        descriptor_layout_count,
        push_constant_size);
    
    VkPipelineShaderStageCreateInfo *shader_infos = r_fill_shader_stage_create_infos(shader_paths, shader_flags);

    /* Is all zero for rendering pipeline shaders */
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    if (binding_info) {
        vertex_input_info.vertexBindingDescriptionCount = binding_info->binding_count;
        vertex_input_info.pVertexBindingDescriptions = binding_info->binding_descriptions;
        vertex_input_info.vertexAttributeDescriptionCount = binding_info->attribute_count;
        vertex_input_info.pVertexAttributeDescriptions = binding_info->attribute_descriptions;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1.0f;

    VkRect2D rect = {};
    rect.extent = r_swapchain_extent();
    
    VkPipelineViewportStateCreateInfo viewport_info {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &rect;

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = cull_mode;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.minSampleShading = 1.0f;

    VkPipelineColorBlendStateCreateInfo blend_info = r_fill_blend_state_info(stage);

    VkDynamicState dynamic_states[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info {};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_info.depthTestEnable = VK_TRUE;
    depth_stencil_info.depthWriteEnable = VK_TRUE;
    depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_info.minDepthBounds = 0.0f;
    depth_stencil_info.maxDepthBounds = 1.0f;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = s_pop_count(shader_flags);
    pipeline_info.pStages = shader_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = &depth_stencil_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = stage->render_pass;
    /* For now just support one subpass per render pass */
    pipeline_info.subpass = 0;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(r_device(), VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));

    r_free_shader_stage_create_info(shader_infos);
    r_free_blend_state_info(&blend_info);

    shader_t shader = {};
    shader.pipeline = pipeline;
    shader.layout = layout;
    shader.flags = shader_flags;

    return shader;
}


shader_t create_3d_shader_color(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags) {
    return s_create_3d_shader(
        binding_info,
        push_constant_size,
        descriptor_layout_types,
        descriptor_layout_count,
        shader_paths,
        shader_flags,
        r_deferred_stage(),
        VK_CULL_MODE_NONE);
}

shader_t create_3d_shader_shadow(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags) {
    return s_create_3d_shader(
        binding_info,
        push_constant_size,
        descriptor_layout_types,
        descriptor_layout_count,
        shader_paths,
        shader_flags,
        r_shadow_stage(),
        VK_CULL_MODE_FRONT_BIT);
}


void foo() {
    VkImageCopy region;
    vkCmdCopyImage((VkCommandBuffer)0, (VkImage)0, (VkImageLayout)0, (VkImage)0, (VkImageLayout)0, 0, &region);
}
