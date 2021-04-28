#include <AL/al.h>
#include "al_load.hpp"
#include "al_sounddb.hpp"
#include <containers.hpp>

namespace al {

static constexpr uint32_t MAX_SOUNDS = 100;

static stack_container_t<sound_t> sounds;

void init_sounddb(uint32_t max_count) {
    sounds.init(max_count);
}

uint32_t register_sound(wav_file_data_t *data) {
    uint32_t id = sounds.add();
    sound_t *s = sounds.get(id);

    alGenBuffers(1, &s->buffer_id);

    uint32_t bytes_per_sample = (float)data->bits_per_sample / 8.0f;
    float length = (float)data->size / ((float)bytes_per_sample * (float)data->channels * (float)data->sample_rate);

    s->duration = length;

    ALenum format;
    
    if (data->channels == 1 && data->bits_per_sample == 8) {
        format = AL_FORMAT_MONO8;
    }
    else if (data->channels == 1 && data->bits_per_sample == 16) {
        format = AL_FORMAT_MONO16;
    }
    else if (data->channels == 2 && data->bits_per_sample == 8) {
        format = AL_FORMAT_STEREO8;
    }
    else if (data->channels == 2 && data->bits_per_sample == 16) {
        format = AL_FORMAT_STEREO16;
    }
    else {
        LOG_ERROR("Unknown wave format\n");
        exit(-1);
    }

    alBufferData(s->buffer_id, format, data->data, data->size, data->sample_rate);

    return id;
}

sound_t *get_sound(uint32_t id) {
    return sounds.get(id);
}

}
