#pragma once

#include "allocators.hpp"

// Allocates a string on free list allocator
inline char *create_fl_string(
    const char *buffer) {
    uint32_t string_length = (uint32_t)strlen(buffer);
    char *new_buffer = FL_MALLOC(char, string_length + 1);

    memcpy(new_buffer, buffer, string_length + 1);

    return new_buffer;
}

inline uint32_t simple_string_hash(
    const char *string) {
    unsigned long hash = 5381;
    uint32_t c;

    while (c = *string++) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

inline uint32_t simple_string_hash(
    const char *string,
    uint32_t length) {
    unsigned long hash = 5381;

    uint32_t current = 0;

    while (current < length) {
        hash = ((hash << 5) + hash) + string[current];

        ++current;
    }

    return hash;
}

inline uint32_t str_to_int(const char *str, uint32_t length) {
    int32_t ret = 0;
    for(int32_t i = 0; i < length; ++i) {
        ret = ret * 10 + (str[i] - '0');
    }
    return ret;
}    
