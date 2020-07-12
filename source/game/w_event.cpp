#include "ai.hpp"
#include <common/log.hpp>
#include "w_internal.hpp"
#include <common/event.hpp>

static context_t *context_ptr;

void w_subscribe_to_events(
    context_t *context,
    listener_t world_listener,
    event_submissions_t *events) {
    context_ptr = context;

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
}

static void s_handle_event_enter_server(
    event_t *event) {
    LOG_INFO("Entering server world\n");

    context_ptr->in_server = 1;

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
    context_ptr->in_meta_menu = 1;
}

static void s_handle_event_leave_server() {
    context_ptr->in_server = 0;
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

    context_ptr->in_meta_menu = 0;
}

static void s_handle_event_new_player(
    event_t *event) {
    event_new_player_t *data = (event_new_player_t *)event->data;

    w_add_player_from_info(&data->info);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(
    event_t *event) {
    if (context_ptr->in_server) {
        event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

        player_t *p = get_player_from_client_id(data->client_id);
            
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
    // context_ptr->track_history = data->value;

    FL_FREE(data);
}

static void s_handle_event_launch_main_menu_screen() {
    context_ptr->in_meta_menu = 1;
    if (!w_get_startup_screen_data()->initialised) {
        w_read_startup_screen();
    }

    w_reposition_spectator();
}

static void s_handle_event_begin_ai_training(
    event_t *event) {
    event_begin_ai_training_t *data = (event_begin_ai_training_t *)event->data;

    context_ptr->in_meta_menu = 0;

    context_ptr->training_type = data->session_type;
    begin_ai_training_population(150);

    w_clear_players();
    w_clear_chunk_world();

    w_begin_ai_training_players(data->session_type);
    w_begin_ai_training_chunks(data->session_type);
}

static void s_handle_event_reset_ai_arena() {
    switch (context_ptr->training_type) {
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

    context_ptr->in_meta_menu = 1;
}

void w_world_event_listener(
    void *object,
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
