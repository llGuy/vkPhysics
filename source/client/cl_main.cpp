#include <cstddef>
#include <ctime>
#include <ux.hpp>
#include "dr_rsc.hpp"
#include "ux_scene.hpp"
#include "wd_core.hpp"
#include "fx_post.hpp"
#include "fx_fade.hpp"
#include "cl_scene.hpp"
#include "cl_event.hpp"
#include "nw_client.hpp"
#include "cl_render.hpp"
#include <vkph_state.hpp>
#include <files.hpp>
#include <vkph_events.hpp>
#include "nw_client_meta.hpp"
#include "nw_client_meta.hpp"
#include <ui_submit.hpp>
#include <app.hpp>
#include <allocators.hpp>

static bool running;

// Core listener of in-game events - main code in cl_event.cpp
static vkph::listener_t core_listener;

/*
  Client's game state is here:
*/
static vkph::state_t *state;

static float dt;

// Starts a fade in effect into the main menu screen
static void s_open() {
    // Launch fade effect immediately
    vkph::event_begin_fade_effect_t *fade_info = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
    fade_info->dest_value = 1.0f;
    fade_info->duration = 6.0f;
    fade_info->trigger_count = 0;
    vkph::submit_event(vkph::ET_BEGIN_FADE, fade_info);
    
    vkph::submit_event(vkph::ET_ENTER_MAIN_MENU_SCENE, NULL);
}

static void s_run() {
    while (running) {
        app::poll_input_events();
        app::translate_input();
        vkph::dispatch_events();

        LN_CLEAR();

        frame_command_buffers_t frame = cl_prepare_frame();

        // Tick whatever scene was bound
        ux::tick_scene(&frame, state);

        // This needs to always happen - quite an important part of the loop
        // Handles transitions between different game modes
        fx_tick_fade_effect();

        cl_finish_frame();
        dt = app::g_delta_time;
    }
}

static void s_parse_command_line_args(
    int32_t argc,
    char *argv[]) {
    enum option_t { O_USER_META_PATH };

    option_t current_option;
    for (uint32_t i = 1; i < argc; ++i) {
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
                nw_set_path_to_user_meta_info(arg);
            } break;
            }
        }
    }
}

// Entry point for client program
int32_t main(
    int32_t argc,
    char *argv[]) {
    global_linear_allocator_init((uint32_t)megabytes(30));
    srand(time(NULL));
    core_listener = set_listener_callback(cl_game_event_listener, NULL);
    running = 1;
    files_init();
    cl_subscribe_to_events(core_listener);

    s_parse_command_line_args(argc, argv);

    state = flmalloc<vkph::state_t>();
    
    app::init_settings();
    vk::init_context();
    ui::init_submission();
    fx_fader_init();
    nw_init(state);

    s_open();

    cl_command_buffers_init();

    dr_resources_init();
    wd_init(state);
    ux::init(state);

    // Initialise scenes
    prepare_scenes(state);

    // Bind main menu
    ux::bind_scene(ST_MAIN, state);

    fx_get_frame_info()->debug_window = 1;

    // Check if the user has registered and can actually join servers
    nw_check_registration();

    s_run();

    vkph::dispatch_events();
    vkph::dispatch_events();

    nw_stop_request_thread();

    return 0;
}

void cl_terminate() {
    running = 0;
    vkph::submit_event(vkph::ET_EXIT_SCENE, NULL);
    vkph::submit_event(vkph::ET_LEAVE_SERVER, NULL);
}

float cl_delta_time() {
    return dt;
}
