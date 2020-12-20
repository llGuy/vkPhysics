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

static uint32_t current_frame = 0;
static uint32_t image_index = 0;

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

// Need to pass the command buffer containing all user interface rendering
void post_process_scene(
    frame_info_t *info,
    VkCommandBuffer ui_command_buffer) {
    r_execute_lighting_pass(primary_command_buffers[image_index]);

    r_execute_motion_blur_pass(primary_command_buffers[image_index], ui_command_buffer);

    if (info->blurred) {
        // Pass this to whatever the last pass is
        r_execute_gaussian_blur_pass(primary_command_buffers[image_index]);
    }

    r_execute_ui_pass(primary_command_buffers[image_index], ui_command_buffer);
}

void end_frame(
    frame_info_t *info) {
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
    
    vkCmdBeginRenderPass(primary_command_buffers[image_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    r_execute_final_pass(primary_command_buffers[image_index]);

#if 1
    if (info->debug_window) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // General stuff
        ImGui::Begin("General");
        ImGui::Text("Framerate: %.1f", ImGui::GetIO().Framerate);

        for (uint32_t i = 0; i < debug_ui_proc_count; ++i) {
            (debug_ui_procs[i])();
        }

        ImGui::End();

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), primary_command_buffers[image_index]);
    }
#endif

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
 
    VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, fences[current_frame]);
    if (result != VK_SUCCESS) {
        LOG_ERROR("FAILED TO SUBMIT WORK TO THE GPU\n");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores[current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain.swapchain;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result != VK_SUCCESS) {
        LOG_ERROR("FAILED TO PRESENT TO SCREEN\n");
    }

    current_frame = (current_frame + 1) % FRAMES_IN_FLIGHT;

    s_update_sensitive_buffer_deletions();
    
    ++frame_id;
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
