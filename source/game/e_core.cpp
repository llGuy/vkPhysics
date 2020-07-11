// Engine core
#include "ai.hpp"
#include "ui.hpp"
#include <cstddef>
#include <time.h>
#include "net.hpp"
#include <stdio.h>
#include "world.hpp"
#include "engine.hpp"
#include "e_internal.hpp"
#include <common/log.hpp>
#include <common/time.hpp>
#include <common/event.hpp>
#include <common/files.hpp>
#include <common/string.hpp>
#include <renderer/input.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

static engine_t engine;

static void s_handle_input() {
    handle_world_input(engine.focus);
    handle_ui_input(engine.focus, &engine.events);
}

// Records a secondary 
static void s_tick(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    (void)render_command_buffer;
    (void)render_shadow_command_buffer;
    (void)transfer_command_buffer;

    tick_net(
        &engine.events);

    tick_world(
        &engine.events);
}

static void s_run_windowed_game() {
    while (engine.running) {
        poll_input_events(&engine.events);
        translate_raw_to_game_input();
        dispatch_events(&engine.events);

        // Linear allocations stay until events get dispatched
        LN_CLEAR();
        ++engine.current_tick;

        s_handle_input();

        engine_current_frame_t frame = e_prepare_frame();

        s_tick(
            frame.render_command_buffer,
            frame.render_shadow_command_buffer,
            frame.transfer_command_buffer);

        e_finish_frame(
            &engine);

        engine.ldelta_time = surface_delta_time();
    }
}

static void s_open(
    game_init_data_t *game_init_data) {
    // Launch fade effect immediately
    event_begin_fade_effect_t *fade_info = FL_MALLOC(event_begin_fade_effect_t, 1);
    fade_info->dest_value = 1.0f;
    fade_info->duration = 6.0f;
    fade_info->trigger_count = 0;
    submit_event(ET_BEGIN_FADE, fade_info, &engine.events);
    
    submit_event(ET_LAUNCH_MAIN_MENU_SCREEN, NULL, &engine.events);

    if (game_init_data->flags & GIF_CLIENT) {
        event_start_client_t *start_client_data = FL_MALLOC(event_start_client_t, 1);
        start_client_data->client_name = "Some shit";
        submit_event(ET_START_CLIENT, start_client_data, &engine.events);
    }
}

static void s_windowed_game_main(
    game_init_data_t *game_init_data) {
    e_subscribe_to_events(engine.game_core_listener, &engine.events);
    engine.focus = HF_UI;

    game_input_settings_init();
    renderer_init();
    e_fader_init();
    ui_init(&engine.events);
    net_init(&engine.events);
    ai_init();

    s_open(game_init_data);

#if LINK_AGAINST_RENDERER
    // TODO: If debugging
    e_register_debug_window_proc(&engine);
#endif

    e_command_buffers_init();
    world_init(&engine.events);

    engine.frame_info.blurred = 0; 
    engine.frame_info.ssao = 0;
    engine.frame_info.debug_window = 1;

    s_run_windowed_game();

    dispatch_events(&engine.events);
}

static time_stamp_t tick_start;
static time_stamp_t tick_end;

static void s_begin_time() {
    tick_start = current_time();
}

static void s_end_time() {
    tick_end = current_time();

    engine.ldelta_time = time_difference(tick_end, tick_start);
}

static void s_run_not_windowed_game() {
    while (engine.running) {
        s_begin_time();

        dispatch_events(&engine.events);

        ++engine.current_tick;
        LN_CLEAR();

        s_tick(
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE);

        // Sleep to not kill CPU usage
        time_stamp_t current = current_time();
        float dt = time_difference(current, tick_start);
        if (dt < 1.0f / 100.0f) {
            sleep_seconds((1.0f / 100.0f) - dt);
        }

        s_end_time();
    }
}

static void s_parse_command_line_args(
    game_init_data_t *game_init_data) {
    if (game_init_data->argc >= 2) {
        event_start_server_t *data = FL_MALLOC(event_start_server_t, 1);
        uint32_t server_name_length = strlen(game_init_data->argv[1]);
        char *server_name = FL_MALLOC(char, server_name_length + 1);
        server_name[server_name_length] = 0;
        strcpy(server_name, game_init_data->argv[1]);
        for (uint32_t c = 0; c < server_name_length; ++c) {
            if (server_name[c] == '_') {
                server_name[c] = ' ';
            }
        }

        data->server_name = server_name;

        submit_event(ET_START_SERVER, data, &engine.events);

    }
    else {
        LOG_ERROR("Incorrect command line arguments, correct usage is (underscores will be replaced by spaces): ./vkPhysics_server My_awesome_server_name\n");
    }
}

static void s_not_windowed_game_main(
    game_init_data_t *game_init_data) {
    world_init(&engine.events);
    net_init(&engine.events);
    ai_init();

    s_parse_command_line_args(game_init_data);

    s_run_not_windowed_game();

    dispatch_events(&engine.events);
}

game_init_flags_t get_game_init_flags() {
    return (game_init_flags_t)engine.init_flags;
}

void game_main(
    game_init_data_t *game_init_data) {
    global_linear_allocator_init((uint32_t)megabytes(30));

    srand(time(NULL));
    
    engine.game_core_listener = set_listener_callback(
        &e_game_event_listener,
        NULL,
        &engine.events);

    engine.running = 1;

    engine.init_flags = game_init_data->flags;

    files_init();

    engine.current_tick = 0;
    if (game_init_data->flags & GIF_WINDOWED) {
        s_windowed_game_main(
            game_init_data);
    }
    else if (game_init_data->flags & GIF_NOT_WINDOWED) {
        s_not_windowed_game_main(
            game_init_data);
    }
    else {
        LOG_ERROR("Couldn't initialise game because of invalid game init flags");
        exit(1);
    }
}

void finish_game() {
    // Deinitialise everything
}

float logic_delta_time() {
    return engine.ldelta_time;
}

uint64_t &get_current_tick() {
    return engine.current_tick;
}

void e_terminate() {
    engine.running = 0;
    submit_event(ET_LEAVE_SERVER, NULL, &engine.events);
}

void e_begin_startup_world_rendering() {
    engine.master_flags.startup = 1;
}

void e_end_startup_world_rendering() {
    engine.master_flags.startup = 0;
}

void e_enter_ui_screen() {
    engine.focus = HF_UI;
    enable_cursor_display();
}

void e_enter_world() {
    engine.focus = HF_WORLD;
    disable_cursor_display();
}

void e_begin_blurred_rendering() {
    engine.frame_info.blurred = 1;
    engine.frame_info.ssao = 0;
}

void e_begin_crisp_rendering() {
    engine.frame_info.blurred = 0;
    engine.frame_info.ssao = 1;
}
