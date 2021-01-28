#include <stdlib.h>
#include <log.hpp>
#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>

#include "al_load.hpp"

namespace al {

static ALCdevice *device;
static ALCcontext *ctx;

bool check_al_errors() {
    ALenum error = alGetError();
    if(error != AL_NO_ERROR) {
        switch(error) {
        case AL_INVALID_NAME:
            printf("AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function\n");
            break;
        case AL_INVALID_ENUM:
            printf("AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function");
            break;
        case AL_INVALID_VALUE:
            printf("AL_INVALID_VALUE: an invalid value was passed to an OpenAL function");
            break;
        case AL_INVALID_OPERATION:
            printf("AL_INVALID_OPERATION: the requested operation is not valid");
            break;
        case AL_OUT_OF_MEMORY:
            printf("AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory");
            break;
        default:
            printf("UNKNOWN AL ERROR: %d", error);
        }

        printf("\n");

        return false;
    }
    return true;
}

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

    wav_file_data_t sample = load_wav_file("sample.wav");

    uint32_t buffer;
    alGenBuffers(1, &buffer);
    check_al_errors();

    ALenum format;
    if (sample.channels == 1 && sample.bits_per_sample == 8) {
        format = AL_FORMAT_MONO8;
    }
    else if (sample.channels == 1 && sample.bits_per_sample == 16) {
        format = AL_FORMAT_MONO16;
    }
    else if (sample.channels == 2 && sample.bits_per_sample == 8) {
        format = AL_FORMAT_STEREO8;
    }
    else if (sample.channels == 2 && sample.bits_per_sample == 16) {
        format = AL_FORMAT_STEREO16;
    }
    else {
        LOG_ERROR("Unknown wave format\n");
        exit(-1);
    }

    alBufferData(buffer, format, sample.data, sample.size, sample.sample_rate);
    check_al_errors();

    uint32_t source;
    alGenSources(1, &source);
    check_al_errors();
    alSourcef(source, AL_PITCH, 1);
    check_al_errors();
    alSourcef(source, AL_GAIN, 1.0f);
    check_al_errors();
    alSource3f(source, AL_POSITION, 0, 0, 0);
    check_al_errors();
    alSource3f(source, AL_VELOCITY, 0, 0, 0);
    check_al_errors();
    alSourcei(source, AL_LOOPING, AL_FALSE);
    check_al_errors();
    alSourcei(source, AL_BUFFER, buffer);
    check_al_errors();

    alSourcePlay(source);
    check_al_errors();
    int32_t state = AL_PLAYING;

    while (state == AL_PLAYING) {
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        check_al_errors();
    }

    LOG_INFO("Finished playing audio file\n");
}

void destroy_context() {
    alcCloseDevice(device);
}

}
