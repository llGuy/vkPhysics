#pragma once

#include <cstdlib>
#include <string.h>
#include "allocators.hpp"

// Allocates a string on free list allocator
inline char *create_fl_string(
    const char *buffer, uint32_t length = 0) {
    uint32_t string_length = (!length) ? (uint32_t)strlen(buffer) : length;
    char *new_buffer = FL_MALLOC(char, string_length + 1);

    memcpy(new_buffer, buffer, string_length + 1);
    new_buffer[string_length] = 0;

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
    for(int32_t i = 0; i < (int32_t)length; ++i) {
        ret = ret * 10 + (str[i] - '0');
    }
    return ret;
}    

inline float str_to_float(const char *str, uint32_t length) {
    char *end = const_cast<char *>(str + length);
    return strtof(str, &end);
}

// Skips characters until one that is equal to c - returns the pointer to the char that is just after the
// character that's equal to c
inline char *skip_to(char *pointer, char c) {
    while (*pointer) {
        if (*pointer == c) {
            return ++pointer;
        }
        else {
            ++pointer;
        }
    }

    if (c == 0) {
        return pointer;
    }
    else {
        return NULL;
    }
}

// Skips characters that are equal to c (consecutive)
inline char *skip_while(char *pointer, char c) {
    while (*pointer) {
        if (*pointer != c) {
            return pointer;
        }
        else {
            ++pointer;
        }
    }
}
