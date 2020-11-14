#include "common/game.hpp"
#include "dr_rsc.hpp"
#include "wd_core.hpp"
#include "wd_event.hpp"
#include "dr_player.hpp"
#include "wd_predict.hpp"
#include <common/log.hpp>
#include <common/player.hpp>
#include <common/constant.hpp>
#include <common/allocators.hpp>

void wd_subscribe_to_events(listener_t world_listener, event_submissions_t *events) {
    subscribe_to_event(ET_ENTER_SERVER, world_listener, events);
    subscribe_to_event(ET_NEW_PLAYER, world_listener, events);
    subscribe_to_event(ET_LEAVE_SERVER, world_listener, events);
    subscribe_to_event(ET_PLAYER_DISCONNECTED, world_listener, events);
    subscribe_to_event(ET_SPAWN, world_listener, events);
    subscribe_to_event(ET_BEGIN_AI_TRAINING, world_listener, events);
    subscribe_to_event(ET_RESET_AI_ARENA, world_listener, events);
    subscribe_to_event(ET_FINISH_GENERATION, world_listener, events);
}

static void s_handle_event_enter_server(
    event_t *event) {
    LOG_INFO("Entering server world\n");

    wd_set_i_am_in_server(1);

    // Reinitialise chunks / players
    wd_clear_world();

    event_enter_server_t *data = (event_enter_server_t *)event->data;

    for (uint32_t i = 0; i < data->info_count; ++i) {
        player_t *player = add_player();
        fill_player_info(player, &data->infos[i]);

        if (player->flags.is_local) {
            wd_set_local_player(player->local_id);

            player->cached_player_action_count = 0;
            player->cached_player_actions = FL_MALLOC(player_action_t, PLAYER_MAX_ACTIONS_COUNT * 2);

            player->flags.is_remote = 0;
            player->flags.is_local = 1;
        }
        else {
            player->flags.is_remote = 1;
            player->flags.is_local = 0;

            // Initialise remote snapshots
            player->remote_snapshots.init();
            player->elapsed = 0.0f;
        }

        game_add_player_to_team(player, (team_color_t)player->flags.team_color);

        player->render = dr_player_render_init();
        dr_player_animated_instance_init(&player->render->animations);
    }

    FL_FREE(data->infos);
    FL_FREE(event->data);
}

// static void s_handle_event_enter_server_meta_menu() {
//     context_ptr->in_meta_menu = 1;
// }

static void s_handle_event_leave_server() {
    wd_set_i_am_in_server(0);

    wd_clear_world();
}

static void s_handle_event_spawn(
    event_t *event) {
    event_spawn_t *data = (event_spawn_t *)event->data;
    uint32_t id = data->client_id;

    LOG_INFOV("Client %i spawned\n", data->client_id);

    int32_t local_id = translate_client_to_local_id(id);
    player_t *p = get_player(local_id);
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
        p->flags.camera_type = CT_THIRD_PERSON;

        p->camera_distance.set(1, 12.0f, 10.0f, 1.0f);
        p->camera_fov.set(1, 80.0f, 60.0f);
        p->current_camera_up = p->ws_up_vector;
    }
}

static void s_handle_event_new_player(
    event_t *event) {
    event_new_player_t *data = (event_new_player_t *)event->data;

    player_t *player = add_player();
    fill_player_info(player, &data->info);

    if (!player->flags.is_local) {
        player->flags.is_remote = 1;

        player->remote_snapshots.init();
        player->elapsed = 0.0f;
    }

    player->render = dr_player_render_init();
    dr_player_animated_instance_init(&player->render->animations);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(
    event_t *event) {
    if (wd_am_i_in_server()) {
        event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

        int32_t local_id = translate_client_to_local_id(data->client_id);
        player_t *p = get_player(local_id);
            
        if (p) {
            remove_player(p->local_id);
        }

        FL_FREE(event->data);
    }
}

static void s_handle_event_finished_receiving_initial_chunk_data() {
    // w_toggle_mesh_update_wait(0);
}

static void s_handle_event_launch_main_menu_screen() {
    // if (!w_get_startup_screen_data()->initialised) {
    //     w_read_startup_screen();
    // }

    // w_reposition_spectator();
}

static void s_handle_event_begin_ai_training(
    event_t *event) {
#if 0
    event_begin_ai_training_t *data = (event_begin_ai_training_t *)event->data;

    context_ptr->in_meta_menu = 0;

    w_begin_ai_training(data->session_type);
#endif
}

static void s_handle_event_finish_generation() {
    //     w_finish_generation();
}

static void s_handle_event_reset_ai_arena() {
    // w_begin_ai_training_chunks(context_ptr->training_type);
}

void wd_world_event_listener(void *object, event_t *event, event_submissions_t *events) {
    switch(event->type) {

    case ET_ENTER_SERVER: {
        s_handle_event_enter_server(event);
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

    case ET_BEGIN_AI_TRAINING: {
        s_handle_event_begin_ai_training(event);
    } break;

    case ET_FINISH_GENERATION: {
        s_handle_event_finish_generation();
    } break;

    case ET_RESET_AI_ARENA: {
        // s_handle_event_reset_ai_arena();
    } break;

    default: {
    } break;

    }
    
}
