#pragma once

#include <common/tools.hpp>

// Interpolation between player / chunk snapshots
struct chunks_to_interpolate_t {
    float elapsed;
    uint32_t max_modified;
    uint32_t modification_count;
    struct chunk_modifications_t *modifications;
};

void wd_interp_init();
void wd_finish_interp_step();
void wd_chunks_interp_step(float dt);
void wd_player_interp_step(float dt, struct player_t *p);
chunks_to_interpolate_t *wd_get_chunks_to_interpolate();
