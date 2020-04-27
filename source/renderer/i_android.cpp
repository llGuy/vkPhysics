#include "input.hpp"
#include <common/log.hpp>
#include <common/tools.hpp>
#include <common/allocators.hpp>

#if defined (__ANDROID__)

static float sdelta_time = 0.0f;

static void s_create_vulkan_surface_proc(
    struct VkInstance_T *instance,
    struct VkSurfaceKHR_T **surface,
    void *native_window) {
    VkAndroidSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surface_info.window = (ANativeWindow *)native_window;
    if (vkCreateAndroidSurfaceKHR(instance, &surface_info, NULL, surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create android surface\n");
        exit(1);
    }
}

static raw_input_t raw_input = {};

raw_input_t *get_raw_input() {
    return &raw_input;
}

input_interface_data_t input_interface_init() {
    
}

#endif
