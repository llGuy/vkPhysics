#include "cl_frame.hpp"
#include <vkph_events.hpp>
#include <ux_scene.hpp>

#include <vk.hpp>

namespace cl {

// Index to the command buffer that is currently being recorded for submission
static uint32_t cmdbuf_index = 0;

// All of the rendering happens in secondary command buffers
static uint32_t secondary_cmdbuf_count;

constexpr uint32_t MAX_SECONDARY_CMDBUFS = 3;

// Secondary command buffers used for rendering
static VkCommandBuffer render_cmdbufs[MAX_SECONDARY_CMDBUFS];
static VkCommandBuffer render_shadow_cmdbufs[MAX_SECONDARY_CMDBUFS];
static VkCommandBuffer transfer_cmdbufs[MAX_SECONDARY_CMDBUFS];
static VkCommandBuffer ui_cmdbufs[MAX_SECONDARY_CMDBUFS];

static frame_command_buffers_t current_frame;

/* 
  This struct gets passed to rendering so that the renderer can choose the post processing
  passes to use against the scene.
*/
static vk::frame_info_t frame_info;

void init_frame_command_buffers() {
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();

    secondary_cmdbuf_count = swapchain_info.image_count;
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_cmdbufs, secondary_cmdbuf_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, render_shadow_cmdbufs, secondary_cmdbuf_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, transfer_cmdbufs, secondary_cmdbuf_count);
    vk::create_command_buffers(VK_COMMAND_BUFFER_LEVEL_SECONDARY, ui_cmdbufs, secondary_cmdbuf_count);

    cmdbuf_index = 0;
}

frame_command_buffers_t prepare_frame() {
    current_frame.render_cmdbuf = render_cmdbufs[cmdbuf_index];
    VkCommandBufferInheritanceInfo inheritance_info = {};
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_DEFERRED);
    vk::begin_command_buffer(current_frame.render_cmdbuf, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.render_shadow_cmdbuf = render_shadow_cmdbufs[cmdbuf_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_SHADOW);
    vk::begin_command_buffer(current_frame.render_shadow_cmdbuf, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    current_frame.transfer_cmdbuf = transfer_cmdbufs[cmdbuf_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_DEFERRED);
    vk::begin_command_buffer(current_frame.transfer_cmdbuf, 0, &inheritance_info);

    current_frame.ui_cmdbuf = ui_cmdbufs[cmdbuf_index];
    vk::fill_main_inheritance_info(&inheritance_info, vk::ST_UI);
    vk::begin_command_buffer(current_frame.ui_cmdbuf, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance_info);

    return current_frame;
}

static void s_render() {
    vk::frame_t frame = vk::begin_frame();

    ux::scene_info_t *scene_info = ux::get_scene_info();
    vk::eye_3d_info_t *eye_info = &scene_info->eye;
    vk::lighting_info_t *lighting_info = &scene_info->lighting;

    vk::gpu_sync_scene3d_data(frame.cmdbuf, eye_info, lighting_info);
    // All data transfers
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.transfer_cmdbuf);

    vk::begin_shadow_rendering(frame.cmdbuf);
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.render_shadow_cmdbuf);
    vk::end_shadow_rendering(frame.cmdbuf);

    // All rendering
    vk::begin_scene_rendering(frame.cmdbuf);
    vk::submit_secondary_command_buffer(frame.cmdbuf, current_frame.render_cmdbuf);
    vk::end_scene_rendering(frame.cmdbuf);

    vk::post_process_scene(&frame_info, current_frame.ui_cmdbuf);
    // Render UI
    vk::end_frame(&frame_info);
}

void finish_frame() {
    vk::end_command_buffer(current_frame.render_cmdbuf);
    vk::end_command_buffer(current_frame.transfer_cmdbuf);
    vk::end_command_buffer(current_frame.render_shadow_cmdbuf);
    vk::end_command_buffer(current_frame.ui_cmdbuf);

    s_render();

    cmdbuf_index = (cmdbuf_index + 1) % secondary_cmdbuf_count;
}

vk::frame_info_t *get_frame_info() {
    return &frame_info;
}

}
