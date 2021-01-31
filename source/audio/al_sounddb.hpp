#pragma once

#include <stdint.h>

namespace al {

struct wav_file_data_t;

struct sound_t {
    uint32_t buffer_id;
};

void init_sounddb(uint32_t max_count);
uint32_t register_sound(wav_file_data_t *data);
sound_t *get_sound(uint32_t id);

}
