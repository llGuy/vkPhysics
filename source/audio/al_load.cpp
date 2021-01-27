#include <string.h>
#include <files.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

#include "log.hpp"
#include "al_load.hpp"

/*
struct wav_file_data_t {
    uint8_t *data;
    int32_t size;
    int32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
};
*/

wav_file_data_t load_wav_file(const char *path) {
    file_handle_t file = create_file(path, FLF_BINARY);
    file_contents_t c = read_file(file);
    serialiser_t serialiser = {};
    serialiser.data_buffer = c.data;
    serialiser.data_buffer_size = c.size;

    wav_file_data_t data = {};

    { // Load header
        char tmp [4] = {};
        serialiser.deserialise_bytes((uint8_t *)tmp, 4); // RIFF
        if (strcmp(tmp, "RIFF")) {
            LOG_ERRORV("Failed to load .wav file: %s\n", path);

            return data;
        }

        uint32_t size = serialiser.deserialise_uint32();

        serialiser.deserialise_bytes((uint8_t *)tmp, 4);
        if (strcmp(tmp, "WAVE")) {
            LOG_ERRORV("Failed to load .wav file: %s\n", path);

            return data;
        }

        serialiser.deserialise_bytes((uint8_t *)tmp, 4);

        uint32_t data_chunk_size = serialiser.deserialise_uint32();
        uint32_t pcm = serialiser.deserialise_uint16();
        uint32_t channel_count = serialiser.deserialise_uint16();
        uint32_t sample_rate = serialiser.deserialise_uint32();
        // (sample_rate * bits_per_sample * channel_count) / 8
        uint32_t sr_bps_c_div_8 = serialiser.deserialise_uint32();
        serialiser.deserialise_uint16();

        uint32_t bits_per_sample = serialiser.deserialise_uint16();
        serialiser.deserialise_bytes((uint8_t *)tmp, 4); // "data"
        if (strcmp(tmp, "data")) {
            LOG_ERRORV("Failed to load .wav file: %s\n", path);

            return data;
        }

        uint32_t data_size = serialiser.deserialise_uint32();

        data.size = data_size;
        data.sample_rate = sample_rate;
        data.channels = channel_count;
        data.bits_per_sample = bits_per_sample;
    }



    { // Load data
        data.data = flmalloc<uint8_t>(data.size);
        serialiser.deserialise_bytes(data.data, data.size);
    }

    return data;
}
