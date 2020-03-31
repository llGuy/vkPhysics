#pragma once

#include "t_types.hpp"

struct serialiser_t {
    uint32_t data_buffer_size;
    uint8_t *data_buffer;
    uint32_t data_buffer_head = 0;
    
    void init(
        uint32_t max_size);
    
    uint8_t *grow_data_buffer(
        uint32_t bytes);
    
    // Basic serialisation
    void serialise_uint8(
        uint8_t u8);
    
    void serialise_bytes(
        uint8_t *bytes,
        uint32_t size);
    
    void serialise_uint16(
        uint16_t u16);
    
    void serialise_uint32(
        uint32_t u32);
    
    void serialise_uint64(
        uint64_t u64);
    
    void serialise_float32(
        float f32);
    
    void serialise_vector3(
        const vector3_t &v3);
    
    void serialise_string(
        const char *string);

    uint8_t deserialise_uint8();
    
    uint16_t deserialise_uint16();
    
    uint32_t deserialise_uint32();
    
    uint64_t deserialise_uint64();
    
    float deserialise_float32();
    
    vector3_t deserialise_vector3();
    
    const char *deserialise_string();
    
    void deserialise_bytes(
        uint8_t *bytes,
        uint32_t size);
};
