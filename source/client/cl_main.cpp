#include <cstddef>
#include <ctime>
#include <ux.hpp>
#include <al_context.hpp>
#include "cl_render.hpp"
#include "ux_scene.hpp"
#include "cl_game.hpp"
#include <time.hpp>
#include "cl_frame.hpp"
#include "cl_sound3d.hpp"
#include "cl_scene.hpp"
#include "cl_net.hpp"
#include <vkph_state.hpp>
#include <files.hpp>
#include <vkph_events.hpp>
#include "cl_net_meta.hpp"
#include "cl_scene_transition.hpp"
#include <ui_submit.hpp>
#include <app.hpp>
#include <allocators.hpp>
#include "cl_main.hpp"

namespace cl {

static bool running;

// Core listener of in-game events - main code in cl_event.cpp
static vkph::listener_t core_listener;

/*
  Client's game state is here:
*/
static vkph::state_t *state;

static void s_loop() {
    while (running) {
        app::poll_input_events();
        app::translate_input();
        vkph::dispatch_events();

        lnclear();

        frame_command_buffers_t frame = prepare_frame();

        // Tick whatever scene was bound
        ux::tick_scene(&frame, state);

        // This needs to always happen - quite an important part of the loop
        // Handles transitions between different game modes
        tick_scene_transition();

        finish_frame();
    }
}

static void s_parse_command_line_args(
    int32_t argc,
    char *argv[]) {
    enum option_t { O_USER_META_PATH };

    option_t current_option;
    for (int32_t i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            // This is an option
            switch (arg[1]) {
            case 'u': {
                current_option = O_USER_META_PATH;
            } break;
            }
        }
        else {
            // This is information
            switch (current_option) {
            case O_USER_META_PATH: {
                set_path_to_user_meta_info(arg);
            } break;
            }
        }
    }
}

// Starts a fade in effect into the main menu screen
static void s_open() {
    // Launch fade effect immediately
    vkph::event_begin_fade_effect_t *fade_info = flmalloc<vkph::event_begin_fade_effect_t>(1);
    fade_info->dest_value = 1.0f;
    fade_info->duration = 6.0f;
    fade_info->trigger_count = 0;
    vkph::submit_event(vkph::ET_BEGIN_FADE, fade_info);
}

static void s_game_event_listener(
    void *object,
    vkph::event_t *event) {
    switch(event->type) {
        
    case vkph::ET_CLOSED_WINDOW: {
        terminate();
    } break;

    case vkph::ET_RESIZE_SURFACE: {
        auto *data = (vkph::event_surface_resize_t *)event->data;

        vk::resize_render_pipeline(data->width, data->height);

        flfree(data);
    } break;

    case vkph::ET_BEGIN_FADE: {
        auto *data = (vkph::event_begin_fade_effect_t *)event->data;
        
        begin_scene_transition(data);

        flfree(data);
    } break;

    default: {
    } break;
        
    }
}

int32_t run(
    int32_t argc,
    char *argv[]) {
    global_linear_allocator_init((uint32_t)megabytes(30));
    // Random number generators aren't really used so this is good enough
    srand((uint32_t)time(NULL));
    running = 1;
    files_init();

    core_listener = vkph::set_listener_callback(s_game_event_listener, NULL);
    { // Subscribe to the core game events.
        vkph::subscribe_to_event(vkph::ET_CLOSED_WINDOW, core_listener);
        vkph::subscribe_to_event(vkph::ET_RESIZE_SURFACE, core_listener);
        vkph::subscribe_to_event(vkph::ET_BEGIN_FADE, core_listener);
    }

    s_parse_command_line_args(argc, argv);

    state = flmalloc<vkph::state_t>();
    
    app::init_settings();
    vk::init_context();
    al::init_context();
    ui::init_submission();
    init_game_sounds_3d();
    init_scene_transition();
    init_net(state);

    s_open();

    init_frame_command_buffers();

    init_render_resources();
    init_game(state);
    ux::init(state);

    // Initialise scenes
    prepare_scenes(state);

    // Bind main menu
    ux::bind_scene(ST_DEBUG, state);

    get_frame_info()->debug_window = 1;

    // Check if the user has registered and can actually join servers
    check_registration();

    s_loop();

    vkph::dispatch_events();
    vkph::dispatch_events();

    stop_request_thread();

    return 0;
}

void terminate() {
    running = 0;
    vkph::submit_event(vkph::ET_EXIT_SCENE, NULL);
    vkph::submit_event(vkph::ET_LEAVE_SERVER, NULL);
}

}

#include <thread>

// Entry point for client program
int32_t main(
    int32_t argc,
    char *argv[]) {
    return cl::run(argc, argv);
}
