#include "gm_mode.hpp"
#include "fx_post.hpp"
#include "cl_render.hpp"
#include <common/event.hpp>

// Index to the command buffer that is currently being recorded for submission
static uint32_t command_buffer_index = 0;

// All of the rendering happens in secondary command buffers
static uint32_t secondary_command_buffer_count;

#define MAX_SECONDARY_COMMAND_BUFFERS 3

// Secondary command buffers used for rendering
static VkCommandBuffer render_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer render_shadow_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer transfer_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer ui_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];

static frame_command_buffers_t current_frame;

void cl_command_buffers_init() {
    swapchain_information_t swapchain_info = {};
    swapchain_information(&swapchain_info);

    secondary_command_buffer_count = swapchain_info.image_count;
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_command_buffers, secondary_command_buffer_count);
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_shadow_command_buffers, secondary_command_buffer_count);
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, transfer_command_buffers, secondary_command_buffer_count);
    create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, ui_command_buffers, secondary_command_buffer_count);

    command_buffer_index = 0;
}

frame_command_buffers_t cl_prepare_frame() {
    current_frame.render_command_buffer = render_command_buffers[command_buffer_index];
    VkCommandBufferInheritanceInfo inheritance_info = {};
    fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
    begin_command_buffer(current_frame.render_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.render_shadow_command_buffer = render_shadow_command_buffers[command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_SHADOW);
    begin_command_buffer(current_frame.render_shadow_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.transfer_command_buffer = transfer_command_buffers[command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
    begin_command_buffer(current_frame.transfer_command_buffer, 0, &inheritance_info);

    current_frame.ui_command_buffer = ui_command_buffers[command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_UI);
    begin_command_buffer(current_frame.ui_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    return current_frame;
}

static void s_render() {
    frame_info_t *frame_info = fx_get_frame_info();

    VkCommandBuffer final_command_buffer = begin_frame();

    eye_3d_info_t *eye_info = gm_get_eye_info();
    lighting_info_t *lighting_info = gm_get_lighting_info();

    gpu_data_sync(final_command_buffer, eye_info, lighting_info);
    // All data transfers
    submit_secondary_command_buffer(final_command_buffer, current_frame.transfer_command_buffer);

    begin_scene_rendering(final_command_buffer);
    // All rendering
    submit_secondary_command_buffer(final_command_buffer, current_frame.render_command_buffer);
    end_scene_rendering(final_command_buffer);

    post_process_scene(frame_info, current_frame.ui_command_buffer);
    // Render UI
    end_frame(frame_info);
}

void cl_finish_frame() {
    end_command_buffer(current_frame.render_command_buffer);
    end_command_buffer(current_frame.transfer_command_buffer);
    end_command_buffer(current_frame.render_shadow_command_buffer);
    end_command_buffer(current_frame.ui_command_buffer);

    s_render();

    command_buffer_index = (command_buffer_index + 1) % secondary_command_buffer_count;
}
