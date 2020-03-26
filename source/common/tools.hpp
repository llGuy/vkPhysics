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

#define FL_MALLOC(type, n) (type *)malloc_debug(sizeof(type) * (n))
#define FL_FREE(ptr) free_debug(ptr)

#define ALLOCA(type, n) (type *)alloca(sizeof(type) * (n))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
