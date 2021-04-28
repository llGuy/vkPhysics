#include <AL/al.h>
#include "al_source.hpp"
#include "al_play.hpp"

namespace al {

static uint32_t listener;

void init_listener() {
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

void set_listener_velocity(const vector3_t &velocity) {
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

}
