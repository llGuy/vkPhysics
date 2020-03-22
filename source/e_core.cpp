// Engine core

#include <stdio.h>
#include "world.hpp"
#include "engine.hpp"
#include "renderer.hpp"
#include "e_internal.hpp"

// Gonna use these when multithreading gets added
// Logic delta time
static float ldelta_time = 0.0f;
// Surface delta time
static float sdelta_time = 0.0f;

static bool running;

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
        
    }
}

static listener_t game_core_listener;

#define MAX_RENDERED_SCENE_COMMAND_BUFFERS 3

static uint32_t rendered_scene_command_buffer_count;
static VkCommandBuffer rendered_scene_command_buffers[MAX_RENDERED_SCENE_COMMAND_BUFFERS];

static enum highlevel_focus_t {
    HF_WORLD, HF_UI
} focus;

static void s_handle_input() {
    handle_world_input();

    /*switch(focus) {
        
    case HF_WORLD: {
        handle_world_input();
        break;
    }
        
    case HF_UI: {
        break;  
    } 
        
    }*/
}

// Records a secondary 
static void s_tick(
    VkCommandBuffer command_buffer) {
    tick_world(
        command_buffer);
}

static void s_render(
    VkCommandBuffer secondary_command_buffer) {
    VkCommandBuffer final_command_buffer = begin_frame();

    eye_3d_info_t eye_info = create_eye_info();
    lighting_info_t lighting_info = create_lighting_info();
    
    gpu_data_sync(
        final_command_buffer,
        &eye_info,
        &lighting_info);

    begin_scene_rendering(
        final_command_buffer);

    submit_secondary_command_buffer(
        final_command_buffer,
        secondary_command_buffer);

    end_scene_rendering(
        final_command_buffer);

    end_frame();
}

static void s_run_windowed_game() {
    while (running) {
        float now = get_current_time();

        poll_input_events();
        translate_raw_to_game_input();
        dispatch_events();

        s_handle_input();

        static uint32_t command_buffer_index = 0;
        VkCommandBuffer rendered_scene = rendered_scene_command_buffers[command_buffer_index];
        VkCommandBufferInheritanceInfo inheritance_info = {};
        fill_main_inheritance_info(&inheritance_info);
        begin_command_buffer(
            rendered_scene,
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritance_info);

        s_tick(rendered_scene);

        end_command_buffer(rendered_scene);

        s_render(rendered_scene);

        float new_now = get_current_time();
        ldelta_time = sdelta_time = new_now - now;

        command_buffer_index = (command_buffer_index + 1) % rendered_scene_command_buffer_count;
    }
}

static void s_windowed_game_main(
    game_init_data_t *game_init_data) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener);

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

    rendered_scene_command_buffer_count = swapchain_info.image_count;

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        rendered_scene_command_buffers,
        rendered_scene_command_buffer_count);

    world_init();
    
    s_run_windowed_game();
}

static void s_run_not_windowed_game() {
    while (running) {
        s_tick(VK_NULL_HANDLE);
    }
}

static void s_not_windowed_game_main(
    game_init_data_t *game_init_data) {
    world_init();
}

void game_main(
    game_init_data_t *game_init_data) {
    e_event_system_init();
    game_core_listener = set_listener_callback(&s_game_event_listener, NULL);

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

float surface_delta_time() {
    return sdelta_time;
}

float logic_delta_time() {
    return ldelta_time;
}
