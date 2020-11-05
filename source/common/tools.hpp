#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include "t_types.hpp"

inline void *malloc_debug(
    uint32_t size) {
    //printf("DEBUG: Allocated %s\n", size);
    return calloc(size, 1);
}

inline void free_debug(
    void *ptr) {
    //printf("DEBUG: Freed\n");
    free(ptr);
}

#define ALLOCA(type, n) (type *)alloca(sizeof(type) * (n))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct buffer_t {
    void *p;
    size_t size;
};

enum class result_t : uint32_t { FAILURE = 0, SUCCESS = 1 };
