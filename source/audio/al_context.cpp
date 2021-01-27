#include <stdlib.h>
#include <log.hpp>
#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>

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
}

void destroy_context() {
    alcCloseDevice(device);
}

}
