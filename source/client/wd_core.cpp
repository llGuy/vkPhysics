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
        for (uint32_t i = 0; i < g_game->local_rocks.data_count; ++i) {
            rock_t *rock = &g_game->local_rocks[i];

            if (rock->flags.active) {
                // Check if there was collision with players or terrain
                terrain_collision_t collision = {};
                collision.ws_size = vector3_t(0.2f);
                collision.ws_position = rock->position;
                collision.ws_velocity = rock->direction;
                collision.es_position = collision.ws_position / collision.ws_size;
                collision.es_velocity = collision.ws_velocity / collision.ws_size;

                check_ray_terrain_collision(&collision);
                if (collision.detected) {
                    LOG_INFO("Detected collision\n");
                    rock->flags.active = 0;

                    g_game->local_rocks.remove(i);
                }

                tick_rock(rock, g_game->dt);
            }
        }

        for (uint32_t i = 0; i < g_game->remote_rocks.data_count; ++i) {
            rock_t *rock = &g_game->remote_rocks[i];

            if (rock->flags.active) {
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
