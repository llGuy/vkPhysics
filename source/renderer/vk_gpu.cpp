#include "vk_gpu.hpp"
#include "vk_context.hpp"
#include "vk_debug.hpp"

#include <log.hpp>
#include <tools.hpp>
#include <allocators.hpp>
#include <vulkan/vulkan_core.h>

namespace vk {

struct device_extensions_t {
    // If this extension isn't available, stop the program.
    uint32_t needed;
    uint32_t available;

    uint32_t count;
    const char **names;

    inline void set_needed(uint32_t i) {
        needed |= 1 << i;
    }

    inline uint32_t is_needed(uint32_t i) const {
        return needed & (1 << i);
    }
};

static int32_t s_verify_hardware_meets_requirements(const device_extensions_t &extensions, device_extensions_t *used) {
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

    uint32_t required_ext_bits = 0;

    uint32_t required_extensions_left = extensions.count;
    for (uint32_t i = 0; i < available_extension_count && required_extensions_left > 0; ++i) {
        for (uint32_t j = 0; j < extensions.count; ++j) {
            if (!strcmp(extension_properties[i].extensionName, extensions.names[j])) {
                required_ext_bits |= 1 << j;
                used->available |= 1 << j;
                --required_extensions_left;
            }
        }
    }
    
    bool can_run = 1;

    used->count = extensions.count - required_extensions_left;
    used->names = lnmalloc<const char *>(used->count);

    uint32_t used_counter = 0;

    // Make sure it isn't a needed extension.
    for (uint32_t i = 0; i < extensions.count; ++i) {
        bool is_found = required_ext_bits & (1 << i);
        bool is_needed = extensions.is_needed(i);
        if (is_found) {
            used->names[used_counter++] = extensions.names[i];
        }
        else if (is_needed) {
            LOG_ERROR("Could not find a critical extension\n");
            can_run = 0;
        }
        else {
            LOG_WARNINGV("Couldn't find device extension: %s\n", extensions.names[i]);
        }
    }

    bool is_swapchain_supported = can_run;

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
            g_ctx->swapchain_support.available_formats = flmalloc<VkSurfaceFormatKHR>(
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
            g_ctx->swapchain_support.available_present_modes = flmalloc<VkPresentModeKHR>(
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
    enum {
        SWAPCHAIN_EXT_INDEX,
        DEBUG_MARKER_EXT_INDEX,
        INVALID_EXT_INDEX
    };

    const char *requested_ext_names[INVALID_EXT_INDEX] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME
    };

    requested_ext_names[SWAPCHAIN_EXT_INDEX] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    // requested_ext_names[DEBUG_MARKER_EXT_INDEX] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;

    device_extensions_t requested_ext = {};
    requested_ext.count = sizeof(requested_ext_names) / sizeof(requested_ext_names[0]);
    requested_ext.names = requested_ext_names;
    requested_ext.set_needed(SWAPCHAIN_EXT_INDEX);

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_ctx->instance, &device_count, NULL);

    VkPhysicalDevice *devices = ALLOCA(VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(g_ctx->instance, &device_count, devices);

    LOG_INFOV("Found %d devices\n", (int)device_count);

    device_extensions_t used_ext = {};
    
    for (uint32_t i = 0; i < device_count; ++i) {
        g_ctx->hardware = devices[i];

        vkGetPhysicalDeviceProperties(g_ctx->hardware, &g_ctx->hardware_properties);
        LOG_INFOV("Device name: %s\n", g_ctx->hardware_properties.deviceName);
        
        if (s_verify_hardware_meets_requirements(requested_ext, &used_ext)) {
            break;
        }

        memset(&used_ext, 0, sizeof(used_ext));
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
    device_info.enabledExtensionCount = used_ext.count;
    device_info.ppEnabledExtensionNames = used_ext.names;
    device_info.pEnabledFeatures = &device_features;

    VK_CHECK(vkCreateDevice(g_ctx->hardware, &device_info, NULL, &g_ctx->device));

    vkGetDeviceQueue(g_ctx->device, g_ctx->queue_families.graphics_family, 0, &g_ctx->graphics_queue);
    vkGetDeviceQueue(g_ctx->device, g_ctx->queue_families.present_family, 0, &g_ctx->present_queue);
    

#if 1
    if (used_ext.available & 1 << DEBUG_MARKER_EXT_INDEX) {
        LOG_INFO("Initialising debug procs\n");
        init_debug_ext_procs();
    }
#endif
}

void destroy_device() {
    vkDestroyDevice(g_ctx->device, NULL);
}

}
