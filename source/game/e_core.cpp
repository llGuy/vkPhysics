// Engine core
#include "net.hpp"
#include <stdio.h>
#include <imgui.h>
#include "world.hpp"
#include "engine.hpp"
#include "e_internal.hpp"
#include <common/log.hpp>
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
        submit_event(ET_LEAVE_SERVER, NULL, &events);
    } break;

    case ET_RESIZE_SURFACE: {
        event_surface_resize_t *data = (event_surface_resize_t *)event->data;
        handle_resize(data->width, data->height);

        FL_FREE(data);
    } break;

    case ET_PRESSED_ESCAPE: {
        // Handle focus change
        // TODO: Have proper focus sort of stack system
        switch (focus) {
            
        case HF_WORLD: {
            focus = HF_UI;
            enable_cursor_display();
        } break;

        case HF_UI: {
            focus = HF_WORLD;
            disable_cursor_display();
        } break;

        }

    } break;
        
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
    } break;

    case HF_UI: {
    } break;

    }
}

static uint64_t current_tick;

// Records a secondary 
static void s_tick(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    tick_net(
        &events);

    tick_world(
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

        // Linear allocations stay until events get dispatched
        LN_CLEAR();
        ++current_tick;

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

        gpu_sync_world(
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
        data->info.is_local = is_local;
        submit_event(ET_NEW_PLAYER, data, &events);
    }

    auto &ps = DEBUG_get_players();

    for (uint32_t i = 0; i < ps.data_count; ++i) {
        player_t *p = ps.data[i];
        if (p) {
            if (p->is_local) {
                ImGui::Text("- Position: %s", glm::to_string(p->ws_position).c_str());
                ImGui::Text("- Direction: %s", glm::to_string(p->ws_view_direction).c_str());
                vector3_t cc = glm::floor(p->ws_position);
                vector3_t from_origin = (vector3_t)cc;
                vector3_t xs_sized = glm::floor(from_origin / (float)CHUNK_EDGE_LENGTH);
                ImGui::Text("- Chunk coord: %s", glm::to_string((ivector3_t)xs_sized).c_str());
            }
        }
    }


    ImGui::Separator();
    ImGui::Text("-- Net --");
    bool started_client = ImGui::Button("Start client");
    if (started_client) {
        submit_event(ET_START_CLIENT, NULL, &events);
    }

    bool started_server = ImGui::Button("Start server");
    if (started_server) {
        submit_event(ET_START_SERVER, NULL, &events);
    }

    static char address_buffer[50] = {};
    ImGui::InputText("Connect to", address_buffer, sizeof(address_buffer));

    static char name_buffer[50] = {};
    ImGui::InputText("Client name", name_buffer, sizeof(name_buffer));
    
    bool request_connection = ImGui::Button("Request connection");
    if (request_connection) {
        event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);
        memset(data, 0, sizeof(event_data_request_to_join_server_t));
        data->ip_address = address_buffer;
        data->client_name = name_buffer;
        submit_event(ET_REQUEST_TO_JOIN_SERVER, data, &events);
    }

    bool leave_server = ImGui::Button("Disconnect");
    if (leave_server) {
        submit_event(ET_LEAVE_SERVER, NULL, &events);
    }
}
#endif

static void s_windowed_game_main(
    game_init_data_t *game_init_data) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener, &events);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener, &events);
    subscribe_to_event(ET_PRESSED_ESCAPE, game_core_listener, &events);

    focus = HF_UI;
    input_interface_data_t input_interface = input_interface_init();

    game_input_settings_init();

    renderer_init(
        input_interface.application_name,
        input_interface.surface_creation_proc,
        input_interface.window,
        input_interface.surface_width,
        input_interface.surface_height);
    
    net_init(&events);

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
        transfer_command_buffers,
        secondary_command_buffer_count);

    world_init(&events);
    
    s_run_windowed_game();

    dispatch_events(&events);
}

static void s_run_not_windowed_game() {
    while (running) {
        clock_t begin = clock();

        dispatch_events(&events);

        ++current_tick;
        LN_CLEAR();

        s_tick(
            VK_NULL_HANDLE,
            VK_NULL_HANDLE);

        clock_t end = clock();

        clock_t delta = end - begin;
        ldelta_time = (float)(delta / (double)CLOCKS_PER_SEC);
    }
}

static void s_not_windowed_game_main(
    game_init_data_t *game_init_data) {
    (void)game_init_data;
    world_init(&events);
    net_init(&events);

    submit_event(ET_START_SERVER, NULL, &events);

    s_run_not_windowed_game();

    dispatch_events(&events);
}

void game_main(
    game_init_data_t *game_init_data) {
    global_linear_allocator_init((uint32_t)megabytes(30));

    game_core_listener = set_listener_callback(
        &s_game_event_listener,
        NULL,
        &events);

    running = 1;

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
