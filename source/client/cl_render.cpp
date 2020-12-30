#include "fx_post.hpp"
#include "cl_render.hpp"
#include <vkph_events.hpp>

#include <vk.hpp>

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
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();

    secondary_command_buffer_count = swapchain_info.image_count;
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_command_buffers, secondary_command_buffer_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_shadow_command_buffers, secondary_command_buffer_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, transfer_command_buffers, secondary_command_buffer_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, ui_command_buffers, secondary_command_buffer_count);

    command_buffer_index = 0;
}

frame_command_buffers_t cl_prepare_frame() {
    current_frame.render_command_buffer = render_command_buffers[command_buffer_index];
    VkCommandBufferInheritanceInfo inheritance_info = {};
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_DEFERRED);
    vk::begin_command_buffer(current_frame.render_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.render_shadow_command_buffer = render_shadow_command_buffers[command_buffer_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_SHADOW);
    vk::begin_command_buffer(current_frame.render_shadow_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.transfer_command_buffer = transfer_command_buffers[command_buffer_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_DEFERRED);
    vk::begin_command_buffer(current_frame.transfer_command_buffer, 0, &inheritance_info);

    current_frame.ui_command_buffer = ui_command_buffers[command_buffer_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_UI);
    vk::begin_command_buffer(current_frame.ui_command_buffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    return current_frame;
}

static void s_render() {
    vk::frame_info_t *frame_info = fx_get_frame_info();

    vk::frame_t frame = vk::begin_frame();

    vk::eye_3d_info_t *eye_info = sc_get_eye_info();
    vk::lighting_info_t *lighting_info = sc_get_lighting_info();

    vk::gpu_sync_scene3d_data(frame.cmdbuf, eye_info, lighting_info);
    // All data transfers
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.transfer_command_buffer);

    vk::begin_shadow_rendering(frame.cmdbuf);
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.render_shadow_command_buffer);
    vk::end_shadow_rendering(frame.cmdbuf);

    // All rendering
    vk::begin_scene_rendering(frame.cmdbuf);
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.render_command_buffer);
    vk::end_scene_rendering(frame.cmdbuf);

    vk::post_process_scene(frame_info, current_frame.ui_command_buffer);
    // Render UI
    vk::end_frame(frame_info);
}

void cl_finish_frame() {
    vk::end_command_buffer(current_frame.render_command_buffer);
    vk::end_command_buffer(current_frame.transfer_command_buffer);
    vk::end_command_buffer(current_frame.render_shadow_command_buffer);
    vk::end_command_buffer(current_frame.ui_command_buffer);

    s_render();

    command_buffer_index = (command_buffer_index + 1) % secondary_command_buffer_count;
}
