#include "cl_net.hpp"
#include <ux_menu_game.hpp>
#include <vkph_state.hpp>
#include "cl_render.hpp"
#include <vkph_chunk.hpp>
#include <vkph_event_data.hpp>
#include <log.hpp>
#include "cl_game.hpp"
#include "cl_game_predict.hpp"
#include <constant.hpp>
#include <vkph_events.hpp>
#include <allocators.hpp>

namespace cl {

void subscribe_to_game_events(vkph::listener_t world_listener) {
    vkph::subscribe_to_event(vkph::ET_ENTER_SERVER, world_listener);
    vkph::subscribe_to_event(vkph::ET_NEW_PLAYER, world_listener);
    vkph::subscribe_to_event(vkph::ET_LEAVE_SERVER, world_listener);
    vkph::subscribe_to_event(vkph::ET_PLAYER_DISCONNECTED, world_listener);
    vkph::subscribe_to_event(vkph::ET_SPAWN, world_listener);
    vkph::subscribe_to_event(vkph::ET_BEGIN_AI_TRAINING, world_listener);
    vkph::subscribe_to_event(vkph::ET_RESET_AI_ARENA, world_listener);
    vkph::subscribe_to_event(vkph::ET_FINISH_GENERATION, world_listener);
}

static void s_handle_event_enter_server(vkph::event_t *event, vkph::state_t *state) {
    LOG_INFO("Entering server world\n");

    set_i_am_in_server(1);

    // Reinitialise chunks / players
    clear_game(state);

    auto *data = (vkph::event_enter_server_t *)event->data;

    for (uint32_t i = 0; i < data->info_count; ++i) {
        vkph::player_t *player = state->add_player();
        player->init(&data->infos[i], state->client_to_local_id_map);

        if (player->flags.is_local) {
            set_local_player(player->local_id, state);

            player->cached_player_action_count = 0;
            player->cached_player_actions = FL_MALLOC(vkph::player_action_t, vkph::PLAYER_MAX_ACTIONS_COUNT * 2);

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

        state->add_player_to_team(player, (vkph::team_color_t)player->flags.team_color);

        player->render = init_player_render();
        init_player_animated_instance(&player->render->animations);
    }

    FL_FREE(data->infos);
    FL_FREE(event->data);
}

// static void s_handle_event_enter_server_meta_menu() {
//     context_ptr->in_meta_menu = 1;
// }

static void s_handle_event_leave_server(vkph::state_t *state) {
    set_i_am_in_server(0);

    clear_game(state);
}

static void s_handle_event_spawn(vkph::event_t *event, vkph::state_t *state) {
    auto *data = (vkph::event_spawn_t *)event->data;

    uint32_t id = 0;

    if (data->client_id == -1) {
        id = get_local_client_index();
    }
    else {
        id = data->client_id;
    }

    LOG_INFOV("Client %i spawned\n", data->client_id);

    int32_t local_id = state->get_local_id(id);
    vkph::player_t *p = state->get_player(local_id);
    p->ws_position = p->next_random_spawn_position;
    p->ws_view_direction = glm::normalize(-p->ws_position);
    // Calculate up vector
    vector3_t right = glm::cross(p->ws_view_direction, vector3_t(0.0f, 1.0f, 0.0f));
    p->ws_up_vector = glm::cross(right, p->ws_view_direction);
    p->flags.is_alive = true;
    // Meteorite when player spawns
    p->flags.interaction_mode = vkph::PIM_METEORITE;
    p->ws_velocity = vector3_t(0.0f);
    p->health = 200;

    if (p->flags.is_local) {
        p->flags.is_third_person = true;

        p->camera_distance.set(1, 12.0f, 10.0f, 1.0f);
        p->camera_fov.set(1, 80.0f, 60.0f);
        p->current_camera_up = p->ws_up_vector;
    }
}

static void s_handle_event_new_player(vkph::event_t *event, vkph::state_t *state) {
    auto *data = (vkph::event_new_player_t *)event->data;

    vkph::player_t *player = state->add_player();
    player->init(&data->info, state->client_to_local_id_map);

    if (!player->flags.is_local) {
        player->flags.is_remote = 1;

        player->remote_snapshots.init();
        player->elapsed = 0.0f;
    }

    player->render = init_player_render();
    init_player_animated_instance(&player->render->animations);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(vkph::event_t *event, vkph::state_t *state) {
    if (am_i_in_server()) {
        auto *data = (vkph::event_player_disconnected_t *)event->data;

        int32_t local_id = state->get_local_id(data->client_id);
        vkph::player_t *p = state->get_player(local_id);

        if (p->idx_in_chunk_list > -1) {
            vkph::chunk_t *chunk = state->access_chunk(p->chunk_coord);
            chunk->players_in_chunk.remove(p->idx_in_chunk_list);
            p->idx_in_chunk_list = -1;
        }

        uint32_t team_color = p->flags.team_color;

        if (team_color != vkph::team_color_t::INVALID) {
            // Remove player from team
            state->remove_player_from_team(p);
            ux::init_game_menu_for_server(state);
        }
            
        if (p) {
            state->remove_player(p->local_id);
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

static void s_handle_event_begin_ai_training(vkph::event_t *event) {
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

void game_event_listener(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch(event->type) {

    case vkph::ET_ENTER_SERVER: {
        s_handle_event_enter_server(event, state);
    } break;

    case vkph::ET_LEAVE_SERVER: {
        s_handle_event_leave_server(state);
    } break;

    case vkph::ET_SPAWN: {
        s_handle_event_spawn(event, state);
    } break;
        
    case vkph::ET_NEW_PLAYER: {
        s_handle_event_new_player(event, state);
    } break;

    case vkph::ET_PLAYER_DISCONNECTED: {
        s_handle_event_player_disconnected(event, state);
    } break;

    case vkph::ET_BEGIN_AI_TRAINING: {
        s_handle_event_begin_ai_training(event);
    } break;

    case vkph::ET_FINISH_GENERATION: {
        s_handle_event_finish_generation();
    } break;

    case vkph::ET_RESET_AI_ARENA: {
        // s_handle_event_reset_ai_arena();
    } break;

    default: {
    } break;

    }
    
}

}
