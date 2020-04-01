// Engine core
#include "net.hpp"
#include <stdio.h>
#include "world.hpp"
#include "engine.hpp"
#include "e_internal.hpp"
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

// Gonna use these when multithreading gets added
// Logic delta time
static float ldelta_time = 0.0f;

static bool running;

static event_submissions_t events = {};

static enum highlevel_focus_t {
    HF_WORLD, HF_UI
} focus;

static void s_game_event_listener(
    void *object,
    event_t *event) {
    switch(event->type) {
        
    case ET_CLOSED_WINDOW: {
        running = 0;
        break;
    }

    case ET_RESIZE_SURFACE: {
        event_surface_resize_t *data = (event_surface_resize_t *)event->data;
        handle_resize(data->width, data->height);
        break;
    }

    case ET_PRESSED_ESCAPE: {
        // Handle focus change
        // TODO: Have proper focus sort of stack system
        switch (focus) {
            
        case HF_WORLD: {
            focus = HF_UI;
            enable_cursor_display();
            break;
        }

        case HF_UI: {
            focus = HF_WORLD;
            disable_cursor_display();
            break;
        }

        }
        
        break;
    }
        
    }
}

static listener_t game_core_listener;

#define MAX_SECONDARY_COMMAND_BUFFERS 3

static uint32_t secondary_command_buffer_count;
static VkCommandBuffer render_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer transfer_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];

static void s_handle_input() {
    switch(focus) {

    case HF_WORLD: {
        handle_world_input();
        break;
    }

    case HF_UI: {
        break;  
    }

    }
}

// Records a secondary 
static void s_tick(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    tick_net(
        &events);

    tick_world(
        render_command_buffer,
        transfer_command_buffer,
        &events);
}

static void s_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    VkCommandBuffer final_command_buffer = begin_frame();

    eye_3d_info_t eye_info = create_eye_info();
    lighting_info_t lighting_info = create_lighting_info();

    gpu_data_sync(
        final_command_buffer,
        &eye_info,
        &lighting_info);

    // All data transfers
    submit_secondary_command_buffer(
        final_command_buffer,
        transfer_command_buffer);
    
    begin_scene_rendering(
        final_command_buffer);

    // All rendering
    submit_secondary_command_buffer(
        final_command_buffer,
        render_command_buffer);

    end_scene_rendering(
        final_command_buffer);

    end_frame();
}

static void s_run_windowed_game() {
    while (running) {
        poll_input_events(&events);
        translate_raw_to_game_input();
        dispatch_events(&events);

        s_handle_input();

        static uint32_t command_buffer_index = 0;
        VkCommandBuffer render_command_buffer = render_command_buffers[command_buffer_index];
        VkCommandBufferInheritanceInfo inheritance_info = {};
        fill_main_inheritance_info(&inheritance_info);
        begin_command_buffer(
            render_command_buffer,
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritance_info);

        VkCommandBuffer transfer_command_buffer = transfer_command_buffers[command_buffer_index];
        fill_main_inheritance_info(&inheritance_info);
        begin_command_buffer(
            transfer_command_buffer,
            0,
            &inheritance_info);

        s_tick(
            render_command_buffer,
            transfer_command_buffer);

        end_command_buffer(render_command_buffer);
        end_command_buffer(transfer_command_buffer);

        s_render(
            render_command_buffer,
            transfer_command_buffer);

        ldelta_time = surface_delta_time();

        command_buffer_index = (command_buffer_index + 1) % secondary_command_buffer_count;
    }
}

static void s_windowed_game_main(
    game_init_data_t *game_init_data) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener, &events);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener, &events);
    subscribe_to_event(ET_PRESSED_ESCAPE, game_core_listener, &events);

    net_init(&events);

    focus = HF_WORLD;
    input_interface_data_t input_interface = input_interface_init();

    game_input_settings_init();

    renderer_init(
        input_interface.application_name,
        input_interface.surface_creation_proc,
        NULL,
        input_interface.window,
        input_interface.surface_width,
        input_interface.surface_height);

    swapchain_information_t swapchain_info = {};
    swapchain_information(&swapchain_info);

    secondary_command_buffer_count = swapchain_info.image_count;

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        render_command_buffers,
        secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        transfer_command_buffers,
        secondary_command_buffer_count);

    world_init();
    
    s_run_windowed_game();
}

static void s_run_not_windowed_game() {
    while (running) {
        s_tick(
            VK_NULL_HANDLE,
            VK_NULL_HANDLE);
    }
}

static void s_not_windowed_game_main(
    game_init_data_t *game_init_data) {
    net_init(&events);
    world_init();
}

void game_main(
    game_init_data_t *game_init_data) {
    global_linear_allocator_init(megabytes(10));

    game_core_listener = set_listener_callback(
        &s_game_event_listener,
        NULL,
        &events);

    running = 1;

    if (game_init_data->flags & GIF_WINDOWED) {
        s_windowed_game_main(
            game_init_data);
    }
    else if (game_init_data->flags & GIF_NOT_WINDOWED) {
        s_not_windowed_game_main(
            game_init_data);
    }
    else {
        ERROR_LOG("Couldn't initialise game because of invalid game init flags");
        exit(1);
    }
}

void finish_game() {
    // Deinitialise everything
}

float logic_delta_time() {
    return ldelta_time;
}
