#include "vk_gpu.hpp"
#include "vk_context.hpp"

#include <log.hpp>
#include <tools.hpp>
#include <allocators.hpp>
#include <vulkan/vulkan_core.h>

namespace vk {

static int32_t s_verify_hardware_meets_requirements(const char **extensions, uint32_t extension_count) {
    // Get queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_ctx->hardware, &queue_family_count, NULL);

    VkQueueFamilyProperties *queue_properties = ALLOCA(VkQueueFamilyProperties, queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_ctx->hardware, &queue_family_count, queue_properties);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_properties[i].queueCount > 0) {
            g_ctx->queue_families.graphics_family = i;
        }

        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_ctx->hardware, i, g_ctx->surface, &present_support);

        if (queue_properties[i].queueCount > 0 && present_support) {
            g_ctx->queue_families.present_family = i;
        }

        if (g_ctx->queue_families.is_complete()) {
            break;
        }
    }

    uint32_t available_extension_count;
    vkEnumerateDeviceExtensionProperties(g_ctx->hardware, NULL, &available_extension_count, NULL);

    VkExtensionProperties *extension_properties = ALLOCA(VkExtensionProperties, available_extension_count);
    vkEnumerateDeviceExtensionProperties(g_ctx->hardware, NULL, &available_extension_count, extension_properties);

    uint32_t required_extensions_left = extension_count;
    for (uint32_t i = 0; i < available_extension_count && required_extensions_left > 0; ++i) {
        for (uint32_t j = 0; j < extension_count; ++j) {
            if (!strcmp(extension_properties[i].extensionName, extensions[j])) {
                --required_extensions_left;
            }
        }
    }
    
    bool is_swapchain_supported = (!required_extensions_left);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(g_ctx->hardware, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(g_ctx->hardware, &device_features);

    bool is_swapchain_usable = 0;
    if (is_swapchain_supported) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx->hardware, g_ctx->surface, &g_ctx->swapchain_support.capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(
            g_ctx->hardware,
            g_ctx->surface,
            &g_ctx->swapchain_support.available_formats_count,
            NULL);

        if (g_ctx->swapchain_support.available_formats_count != 0) {
            g_ctx->swapchain_support.available_formats = FL_MALLOC(
                VkSurfaceFormatKHR,
                g_ctx->swapchain_support.available_formats_count);

            vkGetPhysicalDeviceSurfaceFormatsKHR(
                g_ctx->hardware,
                g_ctx->surface,
                &g_ctx->swapchain_support.available_formats_count,
                g_ctx->swapchain_support.available_formats);
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            g_ctx->hardware,
            g_ctx->surface,
            &g_ctx->swapchain_support.available_present_modes_count,
            NULL);

        if (g_ctx->swapchain_support.available_present_modes_count != 0) {
            g_ctx->swapchain_support.available_present_modes = FL_MALLOC(
                VkPresentModeKHR,
                g_ctx->swapchain_support.available_present_modes_count);

            vkGetPhysicalDeviceSurfacePresentModesKHR(
                g_ctx->hardware,
                g_ctx->surface,
                &g_ctx->swapchain_support.available_present_modes_count,
                g_ctx->swapchain_support.available_present_modes);
        }

        is_swapchain_usable = g_ctx->swapchain_support.available_formats_count &&
            g_ctx->swapchain_support.available_present_modes_count;
    }

    return is_swapchain_supported && is_swapchain_usable
        && device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && g_ctx->queue_families.is_complete()
        && device_features.geometryShader
        && device_features.wideLines
        && device_features.textureCompressionBC
        && device_features.samplerAnisotropy
        && device_features.fillModeNonSolid;
}

static VkFormat s_find_suitable_depth_format(
    VkFormat *formats,
    uint32_t format_count,
    VkImageTiling tiling,
    VkFormatFeatureFlags features) {
    for (uint32_t i = 0; i < format_count; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(g_ctx->hardware, formats[i], &properties);
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

void init_device() {
    uint32_t extension_count = 1;
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_ctx->instance, &device_count, NULL);

    VkPhysicalDevice *devices = ALLOCA(VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(g_ctx->instance, &device_count, devices);

    LOG_INFOV("Found %d devices\n", device_count);
    
    for (uint32_t i = 0; i < device_count; ++i) {
        g_ctx->hardware = devices[i];

        vkGetPhysicalDeviceProperties(g_ctx->hardware, &g_ctx->hardware_properties);
        
        LOG_INFOV("Device name: %s\n", g_ctx->hardware_properties.deviceName);
        
        if (s_verify_hardware_meets_requirements(extensions, extension_count)) {
            break;
        }
    }

    assert(g_ctx->hardware != VK_NULL_HANDLE);

    vkGetPhysicalDeviceMemoryProperties(g_ctx->hardware, &g_ctx->hardware_memory_properties);

    VkFormat formats[] = {
        VK_FORMAT_D32_SFLOAT, 
        VK_FORMAT_D32_SFLOAT_S8_UINT, 
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    g_ctx->suitable_hardware_depth_format = s_find_suitable_depth_format(
        formats,
        3,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    uint32_t unique_queue_family_finder = 0;
    unique_queue_family_finder |= 1 << g_ctx->queue_families.graphics_family;
    unique_queue_family_finder |= 1 << g_ctx->queue_families.present_family;
    uint32_t unique_queue_family_count = pop_count(unique_queue_family_finder);

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
    device_info.enabledLayerCount = g_ctx->validation_layer_count;
    device_info.ppEnabledLayerNames = g_ctx->validation_layers;
    device_info.enabledExtensionCount = extension_count;
    device_info.ppEnabledExtensionNames = extensions;
    device_info.pEnabledFeatures = &device_features;

    VK_CHECK(vkCreateDevice(g_ctx->hardware, &device_info, NULL, &g_ctx->device));

    vkGetDeviceQueue(g_ctx->device, g_ctx->queue_families.graphics_family, 0, &g_ctx->graphics_queue);
    vkGetDeviceQueue(g_ctx->device, g_ctx->queue_families.present_family, 0, &g_ctx->present_queue);
    
}

void destroy_device() {
    vkDestroyDevice(g_ctx->device, NULL);
}

}
