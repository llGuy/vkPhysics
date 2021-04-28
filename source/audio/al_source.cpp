#include <AL/al.h>
#include "al_source.hpp"

namespace al {

void source_t::bind_to_sound(const sound_t *sound) {
    alSourcei(this->id, AL_BUFFER, sound->buffer_id);
}

void source_t::set_looping(bool value) {
    alSourcei(this->id, AL_LOOPING, value);
}

void source_t::set_volume(float volume) {
    alSourcef(this->id, AL_GAIN, volume);
}

void source_t::start_playing() {
    alSourcePlay(this->id);
}

void source_t::stop_playing() {
    alSourceStop(this->id);
}

// Used for 3D sounds
void source_t::update_position(const vector3_t &position) {
    alSource3f(this->id, AL_POSITION, position.x, position.y, position.z);
}

void source_t::update_velocity(const vector3_t &velocity) {
    alSource3f(this->id, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

bool source_t::is_playing() {
    int32_t state;
    alGetSourcei(this->id, AL_SOURCE_STATE, &state);

    return state == AL_PLAYING ? true : false;
}

void source_t::play_sound(const sound_t *sound) {
    if (is_playing()) {
        stop_playing();
    }

    bind_to_sound(sound);
    start_playing();
}

source_t init_ui_source() {
    source_t res = {};
    alGenSources(1, &res.id);
    res.update_position(vector3_t(0.0f));
    res.update_velocity(vector3_t(0.0f));
    return res;
}

source_t init_3d_source() {
    source_t res = {};

    alGenSources(1, &res.id);
    alSourcei(res.id, AL_SOURCE_RELATIVE, AL_TRUE);
    // For now, just use these default values
    alSourcef(res.id, AL_ROLLOFF_FACTOR, 2.5f);
    alSourcef(res.id, AL_REFERENCE_DISTANCE, 15);
    alSourcef(res.id, AL_MAX_DISTANCE, 1000);

    return res;
}

}
