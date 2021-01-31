#pragma once

#include <stdint.h>
#include <math.hpp>
#include <al_source.hpp>
#include <vkph_state.hpp>

namespace cl {

// A struct that will be pointed to in the player class
// Contains sources of the player
struct player_audio_t {
    
};

// Contains source of something that is in the 3d world (e.g. explosions)
struct game_source_t {
    al::source_t src;
    vector3_t position;
    bool is_initialised;
};

enum sound_3d_type_t {
    S3DT_HIT,
    S3DT_INVALID
};

void init_game_sounds_3d();

void spawn_sound(sound_3d_type_t type, vkph::state_t *state, const vector3_t &position);

void tick_sound3d();

}
