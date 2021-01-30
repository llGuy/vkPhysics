#pragma once

#include <math.hpp>
#include <stdint.h>

#include "al_sounddb.hpp"

namespace al {

struct source_t {
    uint32_t id;

    void bind_to_sound(const sound_t *sound);

    void set_looping(bool value);
    void start_playing();
    void stop_playing();
    // From 0 to 1
    void set_volume(float volume);

    // Used for 3D sounds
    void update_position(const vector3_t &position);
    void update_velocity(const vector3_t &velocity);

    bool is_playing();

    void play_sound(const sound_t *sound);
};

source_t init_ui_source();
source_t init_3d_source();

}
