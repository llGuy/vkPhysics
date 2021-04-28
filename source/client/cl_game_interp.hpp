#pragma once

#include <vkph_player.hpp>
#include <tools.hpp>
#include <vkph_state.hpp>
#include <net_chunk_tracker.hpp>

namespace cl {

// Interpolation between player / chunk snapshots
struct chunks_to_interpolate_t {
    float elapsed;
    uint32_t max_modified;
    uint32_t modification_count;
    net::chunk_modifications_t *modifications;
};

/*
  Initialises everything that is needed to have interpolation for the remote players
  and the chunk meshes.
 */
void init_game_interpolation();
void finish_interp_step(vkph::state_t *state);
void chunks_interp_step(float dt, vkph::state_t *state);
void player_interp_step(float dt, vkph::player_t *p, vkph::state_t *state);
chunks_to_interpolate_t *get_chunks_to_interpolate();

}
