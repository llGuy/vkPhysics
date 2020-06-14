// Engine core
#include "ui.hpp"
#include <time.h>
#include "net.hpp"
#include <stdio.h>
#include <imgui.h>
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

// Gonna use these when multithreading gets added
// Logic delta time
static float ldelta_time = 0.0f;

static bool running;

static event_submissions_t events = {};

static enum highlevel_focus_t {
    HF_WORLD, HF_UI
} focus;

static frame_info_t frame_info;

static void s_game_event_listener(
    void *object,
    event_t *event) {
    switch(event->type) {
        
    case ET_CLOSED_WINDOW: {
        running = 0;
        submit_event(ET_LEAVE_SERVER, NULL, &events);
    } break;

    case ET_RESIZE_SURFACE: {
        event_surface_resize_t *data = (event_surface_resize_t *)event->data;
        handle_resize(data->width, data->height);

        FL_FREE(data);
    } break;

    case ET_BEGIN_FADE: {
        event_begin_fade_effect_t *data = (event_begin_fade_effect_t *)event->data;
        
        e_begin_fade_effect(data);

        FL_FREE(data);
    } break;

    case ET_PRESSED_ESCAPE: {
        // Handle focus change
        // TODO: Have proper focus sort of stack system

        // switch (focus) {
            
        // case HF_WORLD: {
        //     focus = HF_UI;
        //     enable_cursor_display();
        // } break;

        // case HF_UI: {
        //     focus = HF_WORLD;
        //     disable_cursor_display();
        // } break;

        // }

    } break;

    case ET_LAUNCH_MAIN_MENU_SCREEN: {
        focus = HF_UI;
        frame_info.blurred = 1;
        frame_info.ssao = 0;

        enable_cursor_display();

        // When entering gameplay mode, enable ssao and disable blur
    } break;

    case ET_EXIT_MAIN_MENU_SCREEN: {
        frame_info.blurred = 0;
        frame_info.ssao = 1;
    } break;

    case ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY: {
        focus = HF_WORLD;

        disable_cursor_display();
    } break;
        
    }
}

static listener_t game_core_listener;

#define MAX_SECONDARY_COMMAND_BUFFERS 3

static uint32_t secondary_command_buffer_count;
static VkCommandBuffer render_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer render_shadow_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer transfer_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
static VkCommandBuffer ui_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];

static void s_handle_input() {
    switch(focus) {

    case HF_WORLD: {
        handle_world_input();
    } break;

    case HF_UI: {
    } break;

    }
}

static uint64_t current_tick;

// Records a secondary 
static void s_tick(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    (void)render_command_buffer;
    (void)render_shadow_command_buffer;
    (void)transfer_command_buffer;

    tick_net(
        &events);

    tick_world(
        &events);
}

static void s_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    VkCommandBuffer ui_command_buffer) {
    (void)render_shadow_command_buffer;

    VkCommandBuffer final_command_buffer = begin_frame();

    eye_3d_info_t eye_info = create_eye_info();
    lighting_info_t lighting_info = create_lighting_info();

    tick_ui(
        &events);

    gpu_data_sync(
        final_command_buffer,
        &eye_info,
        &lighting_info);

    // All data transfers
    submit_secondary_command_buffer(
        final_command_buffer,
        transfer_command_buffer);

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
        render_command_buffer);

    end_scene_rendering(
        final_command_buffer);

    post_process_scene(
        &frame_info,
        ui_command_buffer);

    // Render UI
    end_frame(&frame_info);
}

static void s_run_windowed_game() {
    while (running) {
        poll_input_events(&events);
        translate_raw_to_game_input();
        dispatch_events(&events);

        // Linear allocations stay until events get dispatched
        LN_CLEAR();
        ++current_tick;

        s_handle_input();

        static uint32_t command_buffer_index = 0;
        VkCommandBuffer render_command_buffer = render_command_buffers[command_buffer_index];
        VkCommandBufferInheritanceInfo inheritance_info = {};
        fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
        begin_command_buffer(
            render_command_buffer,
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritance_info);

        VkCommandBuffer render_shadow_command_buffer = render_shadow_command_buffers[command_buffer_index];
        fill_main_inheritance_info(&inheritance_info, RPI_SHADOW);
        begin_command_buffer(
            render_shadow_command_buffer,
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritance_info);

        VkCommandBuffer transfer_command_buffer = transfer_command_buffers[command_buffer_index];
        fill_main_inheritance_info(&inheritance_info, RPI_DEFERRED);
        begin_command_buffer(
            transfer_command_buffer,
            0,
            &inheritance_info);

        VkCommandBuffer ui_command_buffer = ui_command_buffers[command_buffer_index];
        fill_main_inheritance_info(&inheritance_info, RPI_UI);
        begin_command_buffer(
            ui_command_buffer,
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritance_info);

        s_tick(
            render_command_buffer,
            render_shadow_command_buffer,
            transfer_command_buffer);

        gpu_sync_world(
            render_command_buffer,
            render_shadow_command_buffer,
            transfer_command_buffer);

        render_submitted_ui(
            transfer_command_buffer,
            ui_command_buffer);

        end_command_buffer(render_command_buffer);
        end_command_buffer(transfer_command_buffer);
        end_command_buffer(render_shadow_command_buffer);
        end_command_buffer(ui_command_buffer);

        e_tick_fade_effect(&events);

        s_render(
            render_command_buffer,
            render_shadow_command_buffer,
            transfer_command_buffer,
            ui_command_buffer);

        ldelta_time = surface_delta_time();
        
        command_buffer_index = (command_buffer_index + 1) % secondary_command_buffer_count;
    }
}

// Will remove this once have own UI system
// For now though, basically just dispatches events
#if LINK_AGAINST_RENDERER
static void s_world_ui_proc() {
    ImGui::Separator();
    ImGui::Text("-- World --");

    static vector3_t position = { 0.0f, 0.0f, 0.0f };
    ImGui::SliderFloat3("Position", &position[0], -100.0f, +100.0f);

    static vector3_t direction = { 1.0f, 0.0f, 0.0f};
    ImGui::SliderFloat3("Direction", &direction[0], -1.0f, +1.0f);

    static vector3_t up = { 0.0f, 1.0f, 0.0f};
    ImGui::SliderFloat3("Up", &up[0], -1.0f, +1.0f);

    static float default_speed = 10.0f;
    ImGui::SliderFloat("Speed", &default_speed, -1.0f, +1.0f);

    static bool is_local = 0;
    ImGui::Checkbox("Local", &is_local);

    bool add_player = ImGui::Button("Add player");
    
    if (add_player) {
        event_new_player_t *data = FL_MALLOC(event_new_player_t, 1);
        memset(data, 0, sizeof(event_new_player_t));
        data->info.ws_position = position;
        data->info.ws_view_direction = direction;
        data->info.ws_up_vector = up;
        data->info.default_speed = default_speed;
        data->info.client_data = NULL;
        data->info.next_random_spawn_position = vector3_t(data->info.ws_position);

        player_flags_t flags;
        flags.u32 = 0;
        flags.is_local = is_local;

        data->info.flags = flags.u32;
        
        submit_event(ET_NEW_PLAYER, data, &events);
    }
    
    bool request_to_spawn = ImGui::Button("Spawn");
    if (request_to_spawn) {
        event_spawn_t *spawn_event = FL_MALLOC(event_spawn_t, 1);

        spawn_event->client_id = get_local_client_index();

        submit_event(ET_SPAWN, spawn_event, &events);
    }

    auto &ps = DEBUG_get_players();

    player_t *spectator = DEBUG_get_spectator();

    ImGui::Text("- Position: %s", glm::to_string(spectator->ws_position).c_str());
    ImGui::Text("- Direction: %s", glm::to_string(spectator->ws_view_direction).c_str());

    for (uint32_t i = 0; i < ps.data_count; ++i) {
        player_t *p = ps.data[i];
        if (p) {
            if (p->flags.is_local) {
                ImGui::Text("- Position: %s", glm::to_string(p->ws_position).c_str());
                ImGui::Text("- Direction: %s", glm::to_string(p->ws_view_direction).c_str());
                ImGui::Text("- Velocity: %s", glm::to_string(p->ws_velocity).c_str());
                vector3_t cc = glm::floor(p->ws_position);
                vector3_t from_origin = (vector3_t)cc;
                vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
                ImGui::Text("- Chunk coord: %s", glm::to_string((ivector3_t)xs_sized).c_str());
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("-- Net --");

    static char name_buffer[50] = {};
    ImGui::InputText("Client name", name_buffer, sizeof(name_buffer));
    
    bool started_client = ImGui::Button("Start client");
    if (started_client) {
        event_start_client_t *start_client_data = FL_MALLOC(event_start_client_t, 1);
        start_client_data->client_name = name_buffer;
        submit_event(ET_START_CLIENT, start_client_data, &events);
    }

    bool started_server = ImGui::Button("Start server");
    if (started_server) {
        event_start_server_t *start_server_data = FL_MALLOC(event_start_server_t, 1);
        start_server_data->server_name = name_buffer;
        submit_event(ET_START_SERVER, start_server_data, &events);
    }

    bool refresh_servers_list = ImGui::Button("Refresh servers list\n");

    if (refresh_servers_list) {
        submit_event(ET_REQUEST_REFRESH_SERVER_PAGE, NULL, &events);
    }

    available_servers_t *servers = get_available_servers();
    if (servers) {
        if (servers->server_count > 0) {
            ImGui::Separator();
            ImGui::Text("-- Online servers: click to join:");
            for (uint32_t i = 0; i < servers->server_count; ++i) {
                game_server_t *gs_ptr = &servers->servers[i];
                bool join = ImGui::Button(gs_ptr->server_name);
                if (join) {
                    // Submit event to join server
                    printf("Requested to join: %s\n", gs_ptr->server_name);

                    char *server_name = create_fl_string(gs_ptr->server_name);
                    event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);
                    data->server_name = server_name;

                    submit_event(ET_REQUEST_TO_JOIN_SERVER, data, &events);
                }
            }
        }
    }

    static char address_buffer[50] = {};
    ImGui::InputText("Or connect to", address_buffer, sizeof(address_buffer));
    
    bool request_connection = ImGui::Button("Request connection");
    if (request_connection) {
        event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);
        memset(data, 0, sizeof(event_data_request_to_join_server_t));
        data->ip_address = address_buffer;
        submit_event(ET_REQUEST_TO_JOIN_SERVER, data, &events);
    }

    bool leave_server = ImGui::Button("Disconnect");
    if (leave_server) {
        submit_event(ET_LEAVE_SERVER, NULL, &events);
    }

    bool write_startup = ImGui::Button("Write Startup");
    if (write_startup) {
        write_startup_screen();
    }
}
#endif

static void s_windowed_game_main(
    game_init_data_t *game_init_data) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener, &events);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener, &events);
    subscribe_to_event(ET_PRESSED_ESCAPE, game_core_listener, &events);
    subscribe_to_event(ET_BEGIN_FADE, game_core_listener, &events);
    subscribe_to_event(ET_FADE_FINISHED, game_core_listener, &events);
    subscribe_to_event(ET_LAUNCH_MAIN_MENU_SCREEN, game_core_listener, &events);
    subscribe_to_event(ET_EXIT_MAIN_MENU_SCREEN, game_core_listener, &events);
    subscribe_to_event(ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY, game_core_listener, &events);

    focus = HF_UI;

    game_input_settings_init();

    renderer_init();

    e_fader_init();

    // Launch fade effect immediately
    event_begin_fade_effect_t *fade_info = FL_MALLOC(event_begin_fade_effect_t, 1);
    fade_info->dest_value = 1.0f;
    fade_info->duration = 6.0f;
    fade_info->trigger_count = 0;
    submit_event(ET_BEGIN_FADE, fade_info, &events);

    ui_init(&events);

    submit_event(ET_LAUNCH_MAIN_MENU_SCREEN, NULL, &events);

    net_init(&events);

    if (game_init_data->flags & GIF_CLIENT) {
        event_start_client_t *start_client_data = FL_MALLOC(event_start_client_t, 1);
        start_client_data->client_name = "Some shit";
        submit_event(ET_START_CLIENT, start_client_data, &events);
    }

#if LINK_AGAINST_RENDERER
    // TODO: If debugging
    add_debug_ui_proc(s_world_ui_proc);
#endif

    swapchain_information_t swapchain_info = {};
    swapchain_information(&swapchain_info);

    secondary_command_buffer_count = swapchain_info.image_count;

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        render_command_buffers,
        secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        render_shadow_command_buffers,
        secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        transfer_command_buffers,
        secondary_command_buffer_count);

    create_command_buffers(
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        ui_command_buffers,
        secondary_command_buffer_count);

    world_init(&events);

    frame_info.blurred = 0; 
    frame_info.ssao = 0;
    frame_info.debug_window = 0;

    s_run_windowed_game();

    dispatch_events(&events);
}

static time_stamp_t tick_start;
static time_stamp_t tick_end;

static void s_time_init() {
}

static void s_begin_time() {
    tick_start = current_time();
}

static void s_end_time() {
    tick_end = current_time();

    ldelta_time = time_difference(tick_end, tick_start);
}

static void s_run_not_windowed_game() {
    s_time_init();

    while (running) {
        s_begin_time();

        dispatch_events(&events);

        ++current_tick;
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

static void s_not_windowed_game_main(
    game_init_data_t *game_init_data) {
    world_init(&events);
    net_init(&events);

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

        submit_event(ET_START_SERVER, data, &events);

        s_run_not_windowed_game();

        dispatch_events(&events);
    }
    else {
        LOG_ERROR("Incorrect command line arguments, correct usage is (underscores will be replaced by spaces): ./vkPhysics_server My_awesome_server_name\n");
    }
}

int32_t init_flags;

game_init_flags_t get_game_init_flags() {
    return (game_init_flags_t)init_flags;
}

void game_main(
    game_init_data_t *game_init_data) {
    global_linear_allocator_init((uint32_t)megabytes(30));

    srand(time(NULL));
    
    game_core_listener = set_listener_callback(
        &s_game_event_listener,
        NULL,
        &events);

    running = 1;

    init_flags = game_init_data->flags;

    files_init();

    current_tick = 0;
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
    return ldelta_time;
}

uint64_t &get_current_tick() {
    return current_tick;
}
