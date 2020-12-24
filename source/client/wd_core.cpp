#include "wd_core.hpp"
#include "cl_main.hpp"
#include "common/weapon.hpp"
#include "wd_event.hpp"
#include "dr_chunk.hpp"
#include "wd_interp.hpp"
#include "wd_predict.hpp"
#include <common/game.hpp>
#include "wd_spectate.hpp"
#include <common/event.hpp>
#include <common/chunk.hpp>
#include <common/player.hpp>

static listener_t world_listener;

static struct {

    uint8_t in_server: 1;
    uint8_t in_meta_menu: 1;
    uint8_t in_gameplay: 1;

} flags;

void wd_init(event_submissions_t *events) {
    world_listener = set_listener_callback(wd_world_event_listener, NULL, events);
    wd_subscribe_to_events(world_listener, events);

    g_game->init_memory();
    wd_create_spectator();
    wd_set_local_player(-1);

    wd_interp_init();

    flags.in_meta_menu = 1;
}

// TODO: Implement
void wd_destroy() {}

void wd_game_input(float dt) {
    wd_handle_local_player_input(dt);
}

void wd_tick(event_submissions_t *events) {
    // Interpolate between the chunk snapshots that were sent by the server
    wd_chunks_interp_step(cl_delta_time());

    // Interpolate between the player snapshots that were sent by the server
    for (uint32_t i = 0; i < g_game->players.data_count; ++i) {
        player_t *player = g_game->get_player(i);
        
        if (player) {
            if (player->flags.is_remote) {
                wd_player_interp_step(cl_delta_time(), player);
            }
        }
    }

    wd_predict_state(events);

    { // Local and remote projectiles (basically predicting the state)
        player_t *local_player = g_game->get_player(wd_get_local_player());

        for (uint32_t i = 0; i < g_game->rocks.list.data_count; ++i) {
            rock_t *rock = &g_game->rocks.list[i];

            if (rock->flags.active) {
                int32_t player_local_id;
                bool collided_with_player = check_projectile_players_collision(rock, &player_local_id);
                bool collided_with_terrain = check_projectile_terrain_collision(rock);

                if (collided_with_player) {
                    // Player need to get dealt some DAMAGE MOUAHAHAH
                    player_t *dst_player = g_game->get_player(player_local_id);
                    dst_player->health -= rock_t::DIRECT_DAMAGE;

                    if (rock->client_id == local_player->client_id) {
                        // Add this player to the list of players that have been hit
                        // So that the server can check whether or not the client actually got hit
                        wd_add_predicted_projectile_hit(dst_player);

                        uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                        uint32_t ref_idx = rock->flags.ref_idx_obj;

                        local_player->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                        local_player->weapons[weapon_idx].active_projs.remove(ref_idx);
                    }

                    rock->flags.active = 0;
                    g_game->rocks.list.remove(i);
                }
                else if (collided_with_terrain) {
                    // Make sure that players within radius get damage
                    rock->flags.active = 0;
                    g_game->rocks.list.remove(i);
                }

                tick_rock(rock, g_game->dt);
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

void wd_clear_world() {
    g_game->clear_players();

    uint32_t chunk_count;
    chunk_t **chunks = g_game->get_active_chunks(&chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        // Destroy the chunk's rendering resources
        if (chunks[i]->flags.active_vertices) {
            dr_destroy_chunk_render(chunks[i]->render);
            chunks[i]->render = NULL;
        }

        destroy_chunk(chunks[i]);

        chunks[i] = NULL;
    }

    g_game->clear_chunks();
}
