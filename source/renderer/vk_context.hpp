#pragma once

#include "vk_gpu.hpp"
#include "vk_imgui.hpp"
#include "vk_buffer.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_present.hpp"
#include "vk_descriptor.hpp"
#include "vk_render_pipeline.hpp"

#include <common/log.hpp>
#include <vulkan/vulkan.h>
#include <common/containers.hpp>

#define VK_CHECK(call) \
    if (call != VK_SUCCESS) { LOG_ERRORV("%s failed\n", #call); }

namespace vk {

typedef void(*surface_proc_t)(
    struct VkInstance_T *instance,
    struct VkSurfaceKHR_T **surface,
    void *window);

constexpr uint32_t FRAMES_IN_FLIGHT = 2;

extern struct ctx_t {
    VkInstance instance;
    bool enable_validation_layers;
    uint32_t validation_layer_count;
    const char **validation_layers;

    VkDevice device;
    VkPhysicalDevice hardware;
    VkPhysicalDevice physics_device;
    queue_families_t queue_families;
    VkPhysicalDeviceMemoryProperties hardware_memory_properties;
    VkPhysicalDeviceProperties hardware_properties;
    VkFormat suitable_hardware_depth_format;

    VkQueue graphics_queue;
    VkQueue present_queue;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    VkSurfaceKHR surface;
    swapchain_support_t swapchain_support;
    swapchain_t swapchain;

    VkCommandPool graphics_command_pool;
    uint32_t primary_command_buffer_count;
    VkCommandBuffer *primary_command_buffers;

    VkSemaphore image_ready_semaphores[FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[FRAMES_IN_FLIGHT];
    VkFence fences[FRAMES_IN_FLIGHT];

    VkDescriptorPool descriptor_pool;
    descriptor_layouts_t descriptor_layouts;

    imgui_data_t debug;

    flexible_stack_container_t<sensitive_gpu_buffer_t> buffer_deletions;

    uint64_t frame_id;
    // This will only oscillate between 0 and 1 (because FRAMES_IN_FLIGHT = 2)
    uint32_t current_frame;
    uint32_t image_index;

    // For all the post processing effects
    render_pipeline_t pipeline;
} *g_ctx;

void allocate_context();
void deallocate_context();

void init_context();

}
