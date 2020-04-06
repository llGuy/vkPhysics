#pragma once

#include "allocators.hpp"

// Allocates a string on free list allocator
inline char *create_fl_string(const char *buffer) {
    uint32_t string_length = (uint32_t)strlen(buffer);
    char *new_buffer = FL_MALLOC(char, string_length + 1);

    memcpy(new_buffer, buffer, string_length + 1);

    return new_buffer;
}
