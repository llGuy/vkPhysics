#include "ui.hpp"
#include "world.hpp"
#include "e_internal.hpp"

static engine_rendering_t rendering;
static engine_current_frame_t current_frame;

void e_command_buffers_init() {
    swapchain_information_t swapchain_info = {};
    swapchain_information(&swapchain_info);

    rendering.secondary_command_buffer_count = swapchain_info.image_count;

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        rendering.render_command_buffers,
        rendering.secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        rendering.render_shadow_command_buffers,
        rendering.secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        rendering.transfer_command_buffers,
        rendering.secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        rendering.ui_command_buffers,
        rendering.secondary_command_buffer_count);
}

engine_current_frame_t e_prepare_frame() {
    current_frame.render_command_buffer = rendering.render_command_buffers[rendering.command_buffer_index];
    VkCommandBufferInheritanceInfo inheritance_info = {};
    fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
    begin_command_buffer(
        current_frame.render_command_buffer,
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        &inheritance_info);

    current_frame.render_shadow_command_buffer = rendering.render_shadow_command_buffers[rendering.command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_SHADOW);
    begin_command_buffer(
        current_frame.render_shadow_command_buffer,
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        &inheritance_info);

    current_frame.transfer_command_buffer = rendering.transfer_command_buffers[rendering.command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
    begin_command_buffer(
        current_frame.transfer_command_buffer,
        0,
        &inheritance_info);

    current_frame.ui_command_buffer = rendering.ui_command_buffers[rendering.command_buffer_index];
    fill_main_inheritance_info(&inheritance_info, RPI_UI);
    begin_command_buffer(
        current_frame.ui_command_buffer,
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        &inheritance_info);

    return current_frame;
}

static void s_render(
    engine_t *engine) {
    VkCommandBuffer final_command_buffer = begin_frame();

    eye_3d_info_t eye_info = create_eye_info();
    lighting_info_t lighting_info = create_lighting_info();

    tick_ui(
        &engine->events);

    gpu_data_sync(
        final_command_buffer,
        &eye_info,
        &lighting_info);

    // All data transfers
    submit_secondary_command_buffer(
        final_command_buffer,
        current_frame.transfer_command_buffer);

#if 0
    begin_shadow_rendering(
        final_command_buffer);

    submit_secondary_command_buffer(
        final_command_buffer,
        render_shadow_command_buffer);

    end_shadow_rendering(
        final_command_buffer);
#endif
    
    begin_scene_rendering(
        final_command_buffer);

    // All rendering
    submit_secondary_command_buffer(
        final_command_buffer,
        current_frame.render_command_buffer);

    end_scene_rendering(
        final_command_buffer);

    post_process_scene(
        &engine->frame_info,
        current_frame.ui_command_buffer);

    // Render UI
    end_frame(&engine->frame_info);
}

void e_finish_frame(
    engine_t *engine) {
    gpu_sync_world(
        engine->master_flags.startup,
        current_frame.render_command_buffer,
        current_frame.render_shadow_command_buffer,
        current_frame.transfer_command_buffer);

    render_submitted_ui(
        current_frame.transfer_command_buffer,
        current_frame.ui_command_buffer);

    end_command_buffer(current_frame.render_command_buffer);
    end_command_buffer(current_frame.transfer_command_buffer);
    end_command_buffer(current_frame.render_shadow_command_buffer);
    end_command_buffer(current_frame.ui_command_buffer);

    e_tick_fade_effect(
        &engine->events);

    s_render(
        engine);

    rendering.command_buffer_index = (rendering.command_buffer_index + 1) % rendering.secondary_command_buffer_count;
}
