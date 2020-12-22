// Renderer core
#include <stdio.h>
#include <imgui.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "input.hpp"
#include "audio.hpp"
#include "renderer.hpp"
#include <GLFW/glfw3.h>
#include "r_internal.hpp"
#include <common/log.hpp>
#include <vulkan/vulkan.h>
#include <common/files.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <common/allocators.hpp>
#include <common/containers.hpp>
#include <vulkan/vulkan_core.h>

void renderer_init() {
    const char *application_name = input_api_init();

    s_instance_init(application_name);

    input_interface_data_t input_interface = input_interface_init();

    s_debug_messenger_init();
    input_interface.surface_creation_proc(instance, &surface, input_interface.window);
    s_device_init();
    s_swapchain_init(input_interface.surface_width, input_interface.surface_height);
    s_command_pool_init();
    s_primary_command_buffers_init();
    s_synchronisation_init();
    s_descriptor_pool_init();
    s_final_render_pass_init();
    s_global_descriptor_layouts_init();
    
    s_imgui_init(input_interface.window);
    
    r_camera_init(input_interface.window);
    r_lighting_init();
    r_pipeline_init();
    r_environment_init();

    s_sensitive_buffer_deletion_init();

    ui_rendering_init();

    audio_init();

    frame_id = 0;
}

void destroy_renderer() {
    FL_FREE(swapchain_support.available_formats);
    FL_FREE(swapchain_support.available_present_modes);
    FL_FREE(swapchain.images);
    FL_FREE(swapchain.image_views);
    FL_FREE(primary_command_buffers);
    FL_FREE(final_framebuffers);
}

void handle_resize(
    uint32_t width,
    uint32_t height) {
    vkDeviceWaitIdle(r_device());

    resize_swapchain(width, height);
    r_handle_resize(width, height);
}

void gpu_data_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye_info,
    lighting_info_t *lighting_info) {
    r_camera_gpu_sync(command_buffer, eye_info);
    r_lighting_gpu_sync(command_buffer, eye_info, lighting_info);
    r_render_environment_to_offscreen_if_updated(command_buffer);
}

vector2_t convert_pixel_to_ndc(
    const vector2_t &pixel_coord) {
    // TODO: When supporting smaller res backbuffer, need to change this
    VkExtent2D swapchain_extent = r_swapchain_extent();
    uint32_t rect2D_width, rect2D_height, rect2Dx, rect2Dy;

    rect2D_width = swapchain_extent.width;
    rect2D_height = swapchain_extent.height;
    rect2Dx = 0;
    rect2Dy = 0;

    vector2_t ndc;

    ndc.x = (float)(pixel_coord.x - rect2Dx) / (float)rect2D_width;
    ndc.y = 1.0f - (float)(pixel_coord.y - rect2Dy) / (float)rect2D_height;

    ndc.x = 2.0f * ndc.x - 1.0f;
    ndc.y = 2.0f * ndc.y - 1.0f;

    return ndc;
}
