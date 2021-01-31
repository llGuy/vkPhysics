#pragma once

#include <stdint.h>

namespace al {

struct wav_file_data_t {
    uint8_t *data;
    int32_t size;
    int32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
};

wav_file_data_t load_wav_file(const char *path);

}
