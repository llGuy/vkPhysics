#include "wd_core.hpp"
#include "cl_main.hpp"
#include <vkph_weapon.hpp>
#include "dr_chunk.hpp"
#include "wd_interp.hpp"
#include "wd_predict.hpp"
#include <vkph_state.hpp>
#include <vkph_chunk.hpp>
#include "wd_spectate.hpp"
#include <vkph_events.hpp>
#include "wd_event.hpp"
#include <vkph_physics.hpp>

static vkph::listener_t world_listener;

static struct {

    uint8_t in_server: 1;
    uint8_t in_meta_menu: 1;
    uint8_t in_gameplay: 1;

} flags;

void wd_init(vkph::state_t *state) {
    world_listener = vkph::set_listener_callback(wd_world_event_listener, state);
    wd_subscribe_to_events(world_listener);

    state->prepare();
    wd_create_spectator();
    wd_set_local_player(-1);

    wd_interp_init();

    flags.in_meta_menu = 1;
}

// TODO: Implement
void wd_destroy() {}

void wd_game_input(float dt, vkph::state_t *state) {
    wd_handle_local_player_input(dt, state);
}

void wd_tick(vkph::state_t *state) {
    // Interpolate between the chunk snapshots that were sent by the server
    wd_chunks_interp_step(cl_delta_time(), state);

    // Interpolate between the player snapshots that were sent by the server
    for (uint32_t i = 0; i < state->players.data_count; ++i) {
        vkph::player_t *player = state->get_player(i);
        
        if (player) {
            if (player->flags.is_remote) {
                wd_player_interp_step(cl_delta_time(), player, state);
            }
        }
    }

    wd_predict_state(state);

    { // Local and remote projectiles (basically predicting the state)
        vkph::player_t *local_player = state->get_player(wd_get_local_player());

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
                        wd_add_predicted_projectile_hit(dst_player, state);

                        uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                        uint32_t ref_idx = rock->flags.ref_idx_obj;

                        local_player->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                        local_player->weapons[weapon_idx].active_projs.remove(ref_idx);
                    }

                    rock->flags.active = 0;
                    state->rocks.list.remove(i);
                }
                else if (collided_with_terrain) {
                    // Make sure that players within radius get damage
                    rock->flags.active = 0;
                    state->rocks.list.remove(i);
                }

                rock->tick(state->dt);
            }
        }
    }
}

void wd_set_i_am_in_server(bool b) {
    flags.in_server = b;
}

bool wd_am_i_in_server() {
    return flags.in_server;
}

void wd_clear_world(vkph::state_t *state) {
    state->clear_players();

    uint32_t chunk_count;
    vkph::chunk_t **chunks = state->get_active_chunks(&chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        // Destroy the chunk's rendering resources
        if (chunks[i]->flags.active_vertices) {
            dr_destroy_chunk_render(chunks[i]->render);
            chunks[i]->render = NULL;
        }

        chunks[i]->destroy();
        flfree(chunks[i]);

        chunks[i] = NULL;
    }

    state->clear_chunks();
}
