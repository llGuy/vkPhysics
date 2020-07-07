# include "ai.hpp"
#include <cstddef>
#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "w_internal.hpp"
#include <common/log.hpp>
#include "common/event.hpp"
#include <common/files.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>
#include <common/serialiser.hpp>

struct world_t {
    struct {
        uint8_t in_server: 1;
        uint8_t in_training: 1;
        // Are you in a menu where you aren't occupying a player
        uint8_t in_meta_menu: 1;
        uint8_t in_gameplay: 1;
    };

    ai_training_session_t training_type;
};

static world_t world;

static void s_handle_event_enter_server(
    event_t *event) {
    LOG_INFO("Entering server world\n");

    world.in_server = 1;

    // Reinitialise chunks / players
    w_clear_players();
    w_clear_chunk_world();

    event_enter_server_t *data = (event_enter_server_t *)event->data;

    for (uint32_t i = 0; i < data->info_count; ++i) {
        w_add_player_from_info(&data->infos[i]);
    }

    FL_FREE(data->infos);
    FL_FREE(event->data);
}

static void s_handle_event_enter_server_meta_menu() {
    world.in_meta_menu = 1;
}

static void s_handle_event_leave_server() {
    world.in_server = 0;
    w_clear_players();
    w_clear_chunk_world();
}

static void s_handle_event_spawn(
    event_t *event) {
    event_spawn_t *data = (event_spawn_t *)event->data;
    uint32_t id = data->client_id;

    LOG_INFOV("Client %i spawned\n", data->client_id);

    player_t *p = get_player_from_client_id(id);
    p->ws_position = p->next_random_spawn_position;
    p->ws_view_direction = glm::normalize(-p->ws_position);
    // Calculate up vector
    vector3_t right = glm::cross(p->ws_view_direction, vector3_t(0.0f, 1.0f, 0.0f));
    p->ws_up_vector = glm::cross(right, p->ws_view_direction);
    p->flags.alive_state = PAS_ALIVE;
    // Meteorite when player spawns
    p->flags.interaction_mode = PIM_METEORITE;
    p->ws_velocity = vector3_t(0.0f);

    if (p->flags.is_local) {
        w_set_local_player(p->local_id);
        p->flags.camera_type = CT_THIRD_PERSON;

        p->camera_distance.set(1, 12.0f, 10.0f, 1.0f);
        p->camera_fov.set(1, 90.0f, 60.0f);
        p->current_camera_up = p->ws_up_vector;
    }

    world.in_meta_menu = 0;
}

static void s_handle_event_new_player(
    event_t *event) {
    event_new_player_t *data = (event_new_player_t *)event->data;

    w_add_player_from_info(&data->info);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(
    event_t *event) {
    if (world.in_server) {
        event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

        player_t *p = w_get_player_from_client_id(data->client_id);
            
        if (p) {
            w_destroy_player(p->local_id);
        }

        FL_FREE(event->data);
    }
}

static void s_handle_event_started_receiving_initial_chunk_data() {
    w_toggle_mesh_update_wait(1);
}

static void s_handle_event_finished_receiving_initial_chunk_data() {
    w_toggle_mesh_update_wait(0);
}

static void s_handle_event_set_chunk_history_tracker(
    event_t *event) {
    event_set_chunk_history_tracker_t *data = (event_set_chunk_history_tracker_t *)event->data;
    assert(0);
    // world.track_history = data->value;

    FL_FREE(data);
}

static void s_handle_event_launch_main_menu_screen() {
    world.in_meta_menu = 1;
    if (!w_get_startup_screen_data()->initialised) {
        w_read_startup_screen();
    }

    w_reposition_spectator();
}

static void s_handle_event_begin_ai_training(
    event_t *event) {
    event_begin_ai_training_t *data = (event_begin_ai_training_t *)event->data;

    world.in_meta_menu = 0;

    world.training_type = data->session_type;
    begin_ai_training_population(150);

    w_clear_players();
    w_clear_chunk_world();

    w_begin_ai_training_players(data->session_type);
    w_begin_ai_training_chunks(data->session_type);
}

static void s_handle_event_reset_ai_arena() {
    switch (world.training_type) {
    case ATS_WALKING: {
        w_begin_ai_training_chunks(ATS_WALKING);
    } break;

    case ATS_ROLLING: {
            
    } break;
    }
}

static void s_handle_event_launch_game_menu_screen() {
    LOG_INFO("Resetting spectator's positions / view direction\n");

    w_reposition_spectator();

    world.in_meta_menu = 1;
}

static void s_world_event_listener(
    void *,
    event_t *event,
    event_submissions_t *events) {
    switch(event->type) {

    case ET_ENTER_SERVER: {
        s_handle_event_enter_server(event);
    } break;

    case ET_ENTER_SERVER_META_MENU: {
        s_handle_event_enter_server_meta_menu();
    } break;

    case ET_LEAVE_SERVER: {
        s_handle_event_leave_server();
    } break;

    case ET_SPAWN: {
        s_handle_event_spawn(event);
    } break;
        
    case ET_NEW_PLAYER: {
        s_handle_event_new_player(event);
    } break;

    case ET_PLAYER_DISCONNECTED: {
        s_handle_event_player_disconnected(event);
    } break;

    case ET_STARTED_RECEIVING_INITIAL_CHUNK_DATA: {
        s_handle_event_started_receiving_initial_chunk_data();
    } break;

    case ET_FINISHED_RECEIVING_INITIAL_CHUNK_DATA: {
        s_handle_event_finished_receiving_initial_chunk_data();
    } break;

    case ET_SET_CHUNK_HISTORY_TRACKER: {
        s_handle_event_set_chunk_history_tracker(event);
    } break;

    case ET_LAUNCH_MAIN_MENU_SCREEN: {
        s_handle_event_launch_main_menu_screen();
    } break;

    case ET_BEGIN_AI_TRAINING: {
        s_handle_event_begin_ai_training(event);
    } break;

    case ET_RESET_AI_ARENA: {
        s_handle_event_reset_ai_arena();
    } break;

    case ET_LAUNCH_GAME_MENU_SCREEN: {
        s_handle_event_launch_game_menu_screen();
    } break;

    default: {
    } break;

    }
}

static listener_t world_listener;

void world_init(
    event_submissions_t *events) {
    world_listener = set_listener_callback(s_world_event_listener, NULL, events);
    subscribe_to_event(ET_ENTER_SERVER, world_listener, events);
    subscribe_to_event(ET_NEW_PLAYER, world_listener, events);
    subscribe_to_event(ET_LEAVE_SERVER, world_listener, events);
    subscribe_to_event(ET_PLAYER_DISCONNECTED, world_listener, events);
#if 0
    subscribe_to_event(ET_CHUNK_VOXEL_PACKET, world_listener, events);
#endif
    subscribe_to_event(ET_STARTED_RECEIVING_INITIAL_CHUNK_DATA, world_listener, events);
    subscribe_to_event(ET_FINISHED_RECEIVING_INITIAL_CHUNK_DATA, world_listener, events);
    subscribe_to_event(ET_SET_CHUNK_HISTORY_TRACKER, world_listener, events);
    subscribe_to_event(ET_SPAWN, world_listener, events);
    subscribe_to_event(ET_LAUNCH_MAIN_MENU_SCREEN, world_listener, events);
    subscribe_to_event(ET_ENTER_SERVER_META_MENU, world_listener, events);
    subscribe_to_event(ET_BEGIN_AI_TRAINING, world_listener, events);
    subscribe_to_event(ET_RESET_AI_ARENA, world_listener, events);
    subscribe_to_event(ET_LAUNCH_GAME_MENU_SCREEN, world_listener, events);

    memset(&world, 0, sizeof(world_t));

    w_players_data_init();
    w_player_world_init();

    w_chunks_data_init();
    w_chunk_world_init(4);

    world.in_meta_menu = 1;
    w_startup_init();
}

void destroy_world() {
    w_clear_chunk_world();
    w_destroy_chunk_data();
}

void handle_world_input(
    highlevel_focus_t focus) {
    if (world.in_meta_menu) {
        w_handle_spectator_mouse_movement();
    }
    else if (focus == HF_WORLD) {
        game_input_t *game_input = get_game_input();
        w_handle_input(game_input, surface_delta_time());
    }
}

static void s_check_training_ai() {
    // switch (world.training_type) {
    // case ATS_ROLLING: {
        
    // } break;
    // case ATS_WALKING: {
    //     vector3_t pos = world.players[0]->ws_position;
    // } break;
    // }
}

void tick_world(
    event_submissions_t *events) {
    (void)events;
    w_tick_chunks(logic_delta_time());
    w_tick_players(events);
}

void gpu_sync_world(
    bool in_startup,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
    if (in_startup) {
        // Render the startup world (which was loaded in from a file)
        w_render_startup_world(render_command_buffer);

        if (render_command_buffer != VK_NULL_HANDLE) {
            render_environment(render_command_buffer);
        }
    }
    else {
        // Render the actual world
        w_players_gpu_sync_and_render(
            render_command_buffer,
            render_shadow_command_buffer,
            transfer_command_buffer);

        w_chunk_gpu_sync_and_render(
            render_command_buffer,
            transfer_command_buffer);
    
        if (render_command_buffer != VK_NULL_HANDLE) {
            render_environment(render_command_buffer);
        }
    }
}

eye_3d_info_t create_eye_info() {
    eye_3d_info_t info = {};

    player_t *player = w_get_local_player();

    if (!player) {
        player = w_get_spectator();
    }
    else if (player->flags.alive_state != PAS_ALIVE) {
        player = w_get_spectator();
    }

    vector3_t view_direction = player->ws_view_direction;

    // if (player->flags.camera_type == CT_FIRST_PERSON) {
    //     info.position = player->ws_position;
    // }
    // else {
    {
        info.position = player->ws_position - player->ws_view_direction * player->camera_distance.current * w_get_player_scale().x;
        info.position += player->current_camera_up * w_get_player_scale() * 2.0f;

        if (player->flags.interaction_mode == PIM_STANDING && player->flags.moving) {
            // Add view bobbing
            static float angle = 0.0f;
            static float right = 0.0f;
            angle += logic_delta_time() * 10.0f;
            right += logic_delta_time() * 5.0f;
            angle = fmod(angle, glm::radians(360.0f));
            right = fmod(right, glm::radians(360.0f));

            vector3_t view_dest = info.position + view_direction;

            vector3_t right_vec = glm::normalize(glm::cross(player->ws_view_direction, player->current_camera_up));
            info.position += player->current_camera_up * glm::sin(angle) * 0.004f + right_vec * glm::sin(right) * 0.002f;
            view_direction = glm::normalize(view_dest - info.position);
        }
    }
    
    info.direction = view_direction;
    info.up = player->current_camera_up;

    info.fov = player->camera_fov.current;
    info.near = 0.01f;
    info.far = 10000.0f;
    info.dt = surface_delta_time();

    return info;
}

lighting_info_t create_lighting_info() {
    lighting_info_t info = {};
    
    info.ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    /*info.ws_light_positions[0] = vector4_t(player.position, 1.0f);
    info.ws_light_directions[0] = vector4_t(player.direction, 0.0f);
    info.light_colors[0] = vector4_t(100.0f);*/
    info.lights_count = 0;

    // for (uint32_t i = 0; i < world.players.data_count; ++i) {
    //     if (world.players[i]->flags.flashing_light) {
    //         info.ws_light_positions[info.lights_count] = vector4_t(world.players[i]->ws_position + world.players[i]->ws_up_vector, 1.0f);
    //         info.ws_light_directions[info.lights_count] = vector4_t(world.players[i]->ws_view_direction, 0.0f);
    //         info.light_colors[info.lights_count++] = vector4_t(100.0f);
    //     }
    // }

    return info;
}

void write_startup_screen() {
    // w_write_startup_screen();
}

void kill_local_player(
    event_submissions_t *events) {
    LOG_INFO("Local player just died\n");

    submit_event(ET_LAUNCH_GAME_MENU_SCREEN, NULL, events);
    submit_event(ET_ENTER_SERVER_META_MENU, NULL, events);

    w_get_local_player()->flags.alive_state = PAS_DEAD;
}
