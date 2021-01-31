#include <stdlib.h>
#include <log.hpp>
#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>

#include "al_load.hpp"
#include "al_sounddb.hpp"
#include "al_play.hpp"
#include "al_source.hpp"

namespace al {

static ALCdevice *device;
static ALCcontext *ctx;

void init_context() {
    { // Enumerate possible devices
        ALboolean en = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    }

    device = alcOpenDevice(NULL);
    if (!device) {
        LOG_ERROR("Failed to create OpenAL device\n");
        exit(-1);
    }

    ctx = alcCreateContext(device, NULL);
    if (!ctx) {
        LOG_ERROR("Failed to create OpenAL context\n");
        exit(-1);
    }

    auto success = alcMakeContextCurrent(ctx);
    if (success != ALC_TRUE) {
        LOG_ERROR("Failed to make OpenAL context current\n");
        exit(-1);
    }

    init_sounddb(50);
    init_listener();

#if 0
    auto wav = load_wav_file("assets/sound/click.wav");
    uint32_t sound_id = register_sound(&wav);
    sound_t *sound = get_sound(sound_id);

    source_t src = init_ui_source();
    src.bind_to_sound(sound);
    src.set_volume(0.4f);

    src.start_playing();

    int32_t state = true;

    while (state) {
        state = src.is_playing();
    }

    LOG_INFO("Finished playing audio file\n");
#endif
}

void destroy_context() {
    alcCloseDevice(device);
}

}
