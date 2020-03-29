#pragma once

#include "t_types.hpp"

struct serialiser_t {
    uint32_t data_buffer_size;
    uint8_t *data_buffer;
    uint32_t data_buffer_head = 0;
    
    void initialize(uint32_t max_size);
    uint8_t *grow_data_buffer(uint32_t bytes);
    
    // Basic serialization
    void serialize_uint8(uint8_t u8);
    void serialize_bytes(uint8_t *bytes, uint32_t size);
    void serialize_uint16(uint16_t u16);
    void serialize_uint32(uint32_t u32);
    void serialize_uint64(uint64_t u64);
    void serialize_float32(float f32);
    void serialize_vector3(const vector3_t &v3);
    void serialize_string(const char *string);

    uint8_t deserialize_uint8();
    uint16_t deserialize_uint16();
    uint32_t deserialize_uint32();
    uint64_t deserialize_uint64();
    float deserialize_float32();
    vector3_t deserialize_vector3();
    const char *deserialize_string();
    void deserialize_bytes(uint8_t *bytes, uint32_t size);
};
