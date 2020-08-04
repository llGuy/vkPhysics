#include "net.hpp"
#include <imgui.h>
#include "world.hpp"
#include "e_internal.hpp"
#include <common/event.hpp>
#include <common/string.hpp>
#include <renderer/renderer.hpp>

static engine_t *engine_ptr;

void e_register_debug_window_proc(
    engine_t *engine) {
    engine_ptr = engine;

#if LINK_AGAINST_RENDERER
    add_debug_ui_proc(e_debug_window);
#endif
}

#if LINK_AGAINST_RENDERER
void e_debug_window() {
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
        data->info.client_name = NULL;
        data->info.next_random_spawn_position = vector3_t(data->info.ws_position);

        player_flags_t flags;
        flags.u32 = 0;
        flags.is_local = is_local;

        data->info.flags = flags.u32;
        
        submit_event(ET_NEW_PLAYER, data, &engine_ptr->events);
    }
    
    bool request_to_spawn = ImGui::Button("Spawn");
    if (request_to_spawn) {
        event_spawn_t *spawn_event = FL_MALLOC(event_spawn_t, 1);

        spawn_event->client_id = get_local_client_index();

        submit_event(ET_SPAWN, spawn_event, &engine_ptr->events);
    }


    ImGui::Separator();
    ImGui::Text("-- Net --");

    static char name_buffer[50] = {};
    ImGui::InputText("Client name", name_buffer, sizeof(name_buffer));
    
    bool started_client = ImGui::Button("Start client");
    if (started_client) {
        event_start_client_t *start_client_data = FL_MALLOC(event_start_client_t, 1);
        start_client_data->client_name = name_buffer;
        submit_event(ET_START_CLIENT, start_client_data, &engine_ptr->events);
    }

    bool started_server = ImGui::Button("Start server");
    if (started_server) {
        event_start_server_t *start_server_data = FL_MALLOC(event_start_server_t, 1);
        start_server_data->server_name = name_buffer;
        submit_event(ET_START_SERVER, start_server_data, &engine_ptr->events);
    }

    bool refresh_servers_list = ImGui::Button("Refresh servers list\n");

    if (refresh_servers_list) {
        submit_event(ET_REQUEST_REFRESH_SERVER_PAGE, NULL, &engine_ptr->events);
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

                    submit_event(ET_REQUEST_TO_JOIN_SERVER, data, &engine_ptr->events);
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
        submit_event(ET_REQUEST_TO_JOIN_SERVER, data, &engine_ptr->events);
    }

    bool leave_server = ImGui::Button("Disconnect");
    if (leave_server) {
        submit_event(ET_LEAVE_SERVER, NULL, &engine_ptr->events);
    }

    bool write_startup = ImGui::Button("Write Startup");
    if (write_startup) {
        write_startup_screen();
    }

    // AI stuff
    ImGui::Separator();
    ImGui::Text("-- AI --");

    bool start_training_walking = ImGui::Button("Train walking");

    if (start_training_walking) {
        event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);

        effect_data->dest_value = 0.0f;
        effect_data->duration = 2.5f;
        effect_data->fade_back = 1;
        effect_data->trigger_count = 2;
        effect_data->triggers[0].trigger_type = ET_BEGIN_AI_TRAINING;

        event_begin_ai_training_t *training_data = FL_MALLOC(event_begin_ai_training_t, 1);
        training_data->session_type = ATS_WALKING;

        effect_data->triggers[0].next_event_data = training_data;

        effect_data->triggers[1].trigger_type = ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY;
        effect_data->triggers[1].next_event_data = NULL;

        submit_event(ET_BEGIN_FADE, effect_data, &engine_ptr->events);
    }

    bool finish_generation = ImGui::Button("Finish generation");

    if (finish_generation) {
        submit_event(ET_FINISH_GENERATION, NULL, &engine_ptr->events);
    }
}
#endif
