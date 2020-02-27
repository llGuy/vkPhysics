#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>

typedef int32_t result_t;
typedef void(*surface_proc_t)(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window);
typedef void(*imgui_proc_t)();

/* Initialise the renderer (graphics API, rendering pipeline, mesh managers, ...) */
void renderer_init(
    const char *application_name,
    surface_proc_t create_surface,
    imgui_proc_t imgui_proc,
    void *window,
    uint32_t window_width,
    uint32_t window_height);

void resize_swapchain();

VkCommandBuffer begin_frame();

void end_frame();

void begin_scene_rendering(
    VkCommandBuffer command_buffer);

void end_scene_rendering(
    VkCommandBuffer command_buffer);

void create_command_buffers(
    VkCommandBufferLevel level, 
    VkCommandBuffer *command_buffers, uint32_t count);

void begin_command_buffer(
    VkCommandBuffer command_buffer,
    VkCommandBufferUsageFlags usage,
    VkCommandBufferInheritanceInfo *inheritance);

void end_command_buffer(
    VkCommandBuffer command_buffer);

VkCommandBuffer begin_simgle_time_command_buffer();

void end_single_time_command_buffer(
    VkCommandBuffer command_buffer);

struct attachment_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkFormat format;
    VkSampler sampler;
};
    
VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties);
