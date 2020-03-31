#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "t_types.hpp"

inline void *malloc_debug(
    uint32_t size) {
    //printf("DEBUG: Allocated %s\n", size);
    return malloc(size);
}

inline void free_debug(
    void *ptr) {
    //printf("DEBUG: Freed\n");
    free(ptr);
}

#define ALLOCA(type, n) (type *)alloca(sizeof(type) * (n))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
