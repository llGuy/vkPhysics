#include "vk_context.hpp"
#include "vk_instance.hpp"

#include <common/log.hpp>
#include <common/tools.hpp>
#include <vulkan/vulkan_core.h>

namespace vk {

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
            LOG_ERRORV("Validation layer %s not supported\n", layers[requested]);
        }
    }

    *count_dst = requested_verified_count;
}

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_PTR s_debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *) {
#if 1
    LOG_ERRORV("Validation layer: %s\n\n", callback_data->pMessage);
#endif
    return 0;
}
#endif

static void s_init_debug_messenger() {
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
    debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.pNext = NULL;
    debug_messenger_info.flags = 0;
    debug_messenger_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = &s_debug_messenger_callback;
    debug_messenger_info.pUserData = NULL;

    PFN_vkCreateDebugUtilsMessengerEXT ptr_vkCreateDebugUtilsMessenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_ctx->instance, "vkCreateDebugUtilsMessengerEXT");
    if (ptr_vkCreateDebugUtilsMessenger(g_ctx->instance, &debug_messenger_info, NULL, &g_ctx->debug_messenger)) {
        LOG_ERROR("Failed to create debug utils messenger\n");
    }
#endif
}

static const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};

void init_instance(const char *application_name) {
    g_ctx->validation_layer_count = 1;
    g_ctx->validation_layers = validation_layers;

#ifdef NDEBUG

    g_ctx->enable_validation_layers = false;

    g_ctx->validation_layer_count = 0;
    g_ctx->validation_layers = NULL;

#else

    g_ctx->enable_validation_layers = true;

    const char *requested_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    s_verify_layer_support(
        requested_validation_layers,
        g_ctx->validation_layer_count,
        g_ctx->validation_layers,
        &g_ctx->validation_layer_count);

#endif

    uint32_t extension_count = 4;
    const char *extensions[] = {
#if defined(_WIN32)
        "VK_KHR_win32_surface",
#elif defined(__ANDROID__)
        "VK_KHR_android_surface"
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
    instance_info.enabledLayerCount = g_ctx->validation_layer_count;
    instance_info.ppEnabledLayerNames = validation_layers;
    instance_info.enabledExtensionCount = extension_count;
    instance_info.ppEnabledExtensionNames = extensions;

    VK_CHECK(vkCreateInstance(&instance_info, NULL, &g_ctx->instance));

    s_init_debug_messenger();
}

void destroy_instance() {
    vkDestroyInstance(g_ctx->instance, NULL);
}

}
