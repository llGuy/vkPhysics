#include "wd_core.hpp"
#include "cl_main.hpp"
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

    player_memory_init();
    wd_create_spectator();
    wd_set_local_player(-1);

    chunk_memory_init();
    wd_interp_init();

    flags.in_meta_menu = 1;
}

// TODO: Implement
void wd_destroy() {}

void wd_input(float dt) {
    wd_handle_local_player_input(dt);
}

void wd_tick(event_submissions_t *events) {
    // Interpolate between the chunk snapshots that were sent by the server
    wd_chunks_interp_step(cl_delta_time());

    // Interpolate between the player snapshots that were sent by the server
    for (uint32_t i = 0; i < get_player_count(); ++i) {
        player_t *player = get_player(i);
            
        if (player) {
            if (player->flags.is_remote) {
                wd_player_interp_step(cl_delta_time(), player);
            }
        }
    }

    wd_predict_state(events);
}

void wd_set_i_am_in_server(bool b) {
    flags.in_server = b;
}

bool wd_am_i_in_server() {
    return flags.in_server;
}

void wd_clear_world() {
    clear_players();

    uint32_t chunk_count;
    chunk_t **chunks = get_active_chunks(&chunk_count);

    for (uint32_t i = 0; i < chunk_count; ++i) {
        // Destroy the chunk's rendering resources
        if (chunks[i]->flags.active_vertices) {
            dr_destroy_chunk_render(chunks[i]->render);
        }

        destroy_chunk(chunks[i]);

        chunks[i] = NULL;
    }

    clear_chunks();
}
