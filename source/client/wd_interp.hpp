#pragma once

#include <vkph_player.hpp>
#include <common/tools.hpp>
#include <vkph_state.hpp>

// Interpolation between player / chunk snapshots
struct chunks_to_interpolate_t {
    float elapsed;
    uint32_t max_modified;
    uint32_t modification_count;
    struct chunk_modifications_t *modifications;
};

void wd_interp_init();
void wd_finish_interp_step(vkph::state_t *state);
void wd_chunks_interp_step(float dt, vkph::state_t *state);
void wd_player_interp_step(float dt, vkph::player_t *p, vkph::state_t *state);
chunks_to_interpolate_t *wd_get_chunks_to_interpolate();
