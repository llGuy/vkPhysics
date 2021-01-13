#include "allocators.hpp"
#include "cl_game_interp.hpp"
#include "cl_main.hpp"
#include "cl_game.hpp"
#include "cl_game_spectate.hpp"
#include "cl_game_predict.hpp"
#include "cl_game_events.hpp"
#include "cl_render.hpp"
#include "vkph_event.hpp"
#include "vkph_event_data.hpp"
#include "vkph_player.hpp"
#include <vkph_weapon.hpp>
#include <vkph_state.hpp>
#include <vkph_chunk.hpp>
#include <vkph_events.hpp>
#include <vkph_physics.hpp>

namespace cl {

static vkph::listener_t game_listener;

static struct {
    uint8_t in_server: 1;
    uint8_t in_meta_menu: 1;
    uint8_t in_gameplay: 1;
} flags;

void init_game(vkph::state_t *state) {
    game_listener = vkph::set_listener_callback(game_event_listener, state);
    subscribe_to_game_events(game_listener);

    state->prepare();
    create_spectator();
    set_local_player(-1, state);

    init_game_interpolation();

    flags.in_meta_menu = 1;
}

void initialise_client_game_session(uint32_t player_info_count, vkph::player_init_info_t *infos, vkph::state_t *state) {
    LOG_INFO("Entering server world\n");

    set_i_am_in_server(1);

    for (uint32_t i = 0; i < player_info_count; ++i) {
        vkph::player_t *player = state->add_player();
        player->init(&infos[i], state->client_to_local_id_map);

        if (player->flags.is_local) {
            set_local_player(player->local_id, state);

            player->cached_player_action_count = 0;
            player->cached_player_actions = flmalloc<vkph::player_action_t>(vkph::PLAYER_MAX_ACTIONS_COUNT * 2);

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
    }
}

void game_input(float dt, vkph::state_t *state, bool is_empty) {
    if (is_empty) {
        push_empty_actions_to_local_player(dt, state);
    }
    else {
        handle_local_player_input(dt, state);
    }
}

void tick_game(vkph::state_t *state) {
    // Interpolate between the chunk snapshots that were sent by the server
    chunks_interp_step(app::g_delta_time, state);

    // Interpolate between the player snapshots that were sent by the server
    for (uint32_t i = 0; i < state->players.data_count; ++i) {
        vkph::player_t *player = state->get_player(i);
        
        if (player) {
            if (player->flags.is_remote) {
                player_interp_step(app::g_delta_time, player, state);
            }
        }
    }

    predict_state(state);

    { // Local and remote projectiles (basically predicting the state)
        vkph::player_t *local_player = state->get_player(get_local_player(state));

        for (uint32_t i = 0; i < state->rocks.list.data_count; ++i) {
            vkph::rock_t *rock = &state->rocks.list[i];

            if (rock->flags.active) {
                int32_t player_local_id;
                bool collided_with_player = vkph::check_projectile_players_collision(rock, &player_local_id, state);
                bool collided_with_terrain = vkph::check_projectile_terrain_collision(rock, state);

                if (collided_with_player) {
                    // Player need to get dealt some DAMAGE MOUAHAHAH
                    vkph::player_t *dst_player = state->get_player(player_local_id);
                    dst_player->health -= vkph::rock_t::DIRECT_DAMAGE;

                    if (rock->client_id == local_player->client_id) {
                        // Add this player to the list of players that have been hit
                        // So that the server can check whether or not the client actually got hit
                        add_predicted_projectile_hit(dst_player, state);

                        uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                        uint32_t ref_idx = rock->flags.ref_idx_obj;

                        local_player->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                        local_player->weapons[weapon_idx].active_projs.remove(ref_idx);
                    }
                    else if (player_local_id == state->local_player_id) {
                        auto *damage_event_data = flmalloc<vkph::event_client_took_damage_t>();
                        vkph::movement_axes_t axes = vkph::compute_movement_axes(
                            dst_player->ws_view_direction,
                            dst_player->ws_up_vector);

                        damage_event_data->bullet_src_dir = glm::normalize(-rock->direction);
                        damage_event_data->view_dir = axes.forward;
                        damage_event_data->up = axes.up;
                        damage_event_data->right = axes.right;

                        vkph::submit_event(vkph::ET_CLIENT_TOOK_DAMAGE, damage_event_data);
                    }

                    rock->flags.active = 0;
                    state->rocks.list.remove(i);
                }
                else if (collided_with_terrain) {
                    // Make sure that players within radius get damage
                    rock->flags.active = 0;
                    state->rocks.list.remove(i);
                }

                rock->tick(state->delta_time);
            }
        }
    }
}

void set_i_am_in_server(bool b) {
    flags.in_server = b;
}

bool am_i_in_server() {
    return flags.in_server;
}

void clear_game(vkph::state_t *state) {
    state->clear_players();

    uint32_t chunk_count;
    vkph::chunk_t **chunks = state->get_active_chunks(&chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        // Destroy the chunk's rendering resources
        if (chunks[i]->flags.active_vertices) {
            destroy_chunk_render(chunks[i]->render);
            chunks[i]->render = NULL;
        }

        chunks[i]->destroy();
        flfree(chunks[i]);

        chunks[i] = NULL;
    }

    state->clear_chunks();
}

}
