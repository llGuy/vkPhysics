#include "tools.hpp"
#include "serialiser.hpp"
#include "allocators.hpp"

void serialiser_t::init(
    uint32_t max_size) {
    data_buffer = (uint8_t *)LN_MALLOC(uint8_t, max_size * sizeof(uint8_t));
}

uint8_t *serialiser_t::grow_data_buffer(
    uint32_t bytes) {
    uint32_t previous = data_buffer_head;
    data_buffer_head += bytes;
    return(&data_buffer[previous]);
}

void serialiser_t::serialise_uint8(
    uint8_t u8) {
    uint8_t *pointer = grow_data_buffer(1);
    *pointer = u8;
}

uint8_t serialiser_t::deserialise_uint8() {
    uint8_t *pointer = grow_data_buffer(1);
    return(*pointer);
}

void serialiser_t::serialise_float32(
    float f32) {
    uint8_t *pointer = grow_data_buffer(4);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    *(float *)pointer = f32;
#else
    uint32_t *f = (uint32_t *)&f32;
    *pointer++ = (uint8_t)*f;
    *pointer++ = (uint8_t)(*f >> 8);
    *pointer++ = (uint8_t)(*f >> 16);
    *pointer++ = (uint8_t)(*f >> 24);
#endif
}

float serialiser_t::deserialise_float32() {
    uint8_t *pointer = grow_data_buffer(4);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    return(*(float *)pointer);
#else
    uint32_t ret = 0;
    ret += (*pointer++);
    ret += ((uint32_t)(*pointer++)) << 8;
    ret += ((uint32_t)(*pointer++)) << 16;
    ret += ((uint32_t)(*pointer++)) << 24;
    
    return(*(float *)(&ret));
#endif
}

void serialiser_t::serialise_vector3(
    const vector3_t &v3) {
    serialise_float32(v3.x);
    serialise_float32(v3.y);
    serialise_float32(v3.z);
}

vector3_t serialiser_t::deserialise_vector3() {
    vector3_t v3 = {};
    v3.x = deserialise_float32();
    v3.y = deserialise_float32();
    v3.z = deserialise_float32();

    return(v3);
}

void serialiser_t::serialise_uint16(
    uint16_t u16) {
    uint8_t *pointer = grow_data_buffer(2);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    *(uint16_t *)pointer = u16;
#else
    *pointer++ = (uint8_t)u16;
    *pointer++ = (uint8_t)(u16 >> 8);
#endif
}

uint16_t serialiser_t::deserialise_uint16() {
    uint8_t *pointer = grow_data_buffer(2);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    return(*(uint16_t *)pointer);
#else
    uint16_t ret = 0;
    ret += (*pointer++);
    ret += ((uint16_t)(*pointer++)) << 8;
    return(ret);
#endif
}

void serialiser_t::serialise_int16(
    int16_t u16) {
    uint8_t *pointer = grow_data_buffer(2);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    *(int16_t *)pointer = u16;
#else
    *pointer++ = (uint8_t)u16;
    *pointer++ = (uint8_t)(u16 >> 8);
#endif
}

int16_t serialiser_t::deserialise_int16() {
    uint8_t *pointer = grow_data_buffer(2);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    return(*(int16_t *)pointer);
#else
    uint16_t ret = 0;
    ret += (*pointer++);
    ret += ((int16_t)(*pointer++)) << 8;
    return(ret);
#endif
}

void serialiser_t::serialise_uint32(
    uint32_t u32) {
    uint8_t *pointer = grow_data_buffer(4);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    *(uint32_t *)pointer = u32;
#else
    *pointer++ = (uint8_t)u32;
    *pointer++ = (uint8_t)(u32 >> 8);
    *pointer++ = (uint8_t)(u32 >> 16);
    *pointer++ = (uint8_t)(u32 >> 24);
#endif
}

uint32_t serialiser_t::deserialise_uint32() {
    uint8_t *pointer = grow_data_buffer(4);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    return(*(uint32_t *)pointer);
#else
    uint32_t ret = 0;
    ret += (*pointer++);
    ret += ((uint32_t)(*pointer++)) << 8;
    ret += ((uint32_t)(*pointer++)) << 16;
    ret += ((uint32_t)(*pointer++)) << 24;
    return(ret);
#endif
}

void serialiser_t::serialise_uint64(
    uint64_t u64) {
    uint8_t *pointer = grow_data_buffer(8);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    *(uint64_t *)pointer = u64;
#else
    *pointer++ = (uint8_t)u32;
    *pointer++ = (uint8_t)(u32 >> 8);
    *pointer++ = (uint8_t)(u32 >> 16);
    *pointer++ = (uint8_t)(u32 >> 24);
    *pointer++ = (uint8_t)(u32 >> 32);
    *pointer++ = (uint8_t)(u32 >> 40);
    *pointer++ = (uint8_t)(u32 >> 48);
    *pointer++ = (uint8_t)(u32 >> 56);
#endif
}

uint64_t serialiser_t::deserialise_uint64() {
    uint8_t *pointer = grow_data_buffer(8);
#if defined (__i386) || defined (__x86_64__) || defined (_M_IX86) || defined(_M_X64)
    return(*(uint64_t *)pointer);
#else
    uint64_t ret = 0;
    ret += (*pointer++);
    ret += ((uint64_t)(*pointer++)) << 8;
    ret += ((uint64_t)(*pointer++)) << 16;
    ret += ((uint64_t)(*pointer++)) << 24;
    ret += ((uint64_t)(*pointer++)) << 32;
    ret += ((uint64_t)(*pointer++)) << 40;
    ret += ((uint64_t)(*pointer++)) << 48;
    ret += ((uint64_t)(*pointer++)) << 56;
    return(ret);
#endif
}

void serialiser_t::serialise_string(
    const char *string) {
    uint32_t string_length = (uint32_t)strlen(string);
    
    uint8_t *pointer = grow_data_buffer((uint32_t)strlen(string) + 1);
    memcpy(pointer, string, string_length + 1);
}


const char *serialiser_t::deserialise_string() {
    uint8_t *pointer = &data_buffer[data_buffer_head];
    uint32_t string_length = (uint32_t)strlen((char *)pointer);
    grow_data_buffer(string_length + 1);

    char *ret = (char *)LN_MALLOC(char, string_length + 1);
    memcpy(ret, pointer, string_length + 1);
    return(ret);
}

void serialiser_t::serialise_bytes(
    uint8_t *bytes,
    uint32_t size) {
    uint8_t *pointer = grow_data_buffer(size);
    memcpy(pointer, bytes, size);
}


uint8_t *serialiser_t::deserialise_bytes(
    uint8_t *bytes,
    uint32_t size) {
    uint8_t *pointer = grow_data_buffer(size);

    if (bytes) {
        memcpy(bytes, pointer, size);
    }

    return pointer;
}
