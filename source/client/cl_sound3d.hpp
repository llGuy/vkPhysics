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

    S3DT_HARD_STEP0,
    S3DT_HARD_STEP1,
    S3DT_HARD_STEP2,
    S3DT_HARD_STEP3,
    S3DT_HARD_STEP4,

    S3DT_INVALID
};

void init_game_sounds_3d();
game_source_t *get_game_source(uint32_t handle);

// One-time sound (for like explosions or something). No need to pass a handle to it
void spawn_sound(sound_3d_type_t type, vkph::state_t *state, const vector3_t &position);

// Composed sound = sound which is composed of many different sounds that
// loop between eachother
uint32_t spawn_composed_sound(sound_3d_type_t type0, uint32_t count, float sound_per_sec);

void tick_sound3d();

}
