#pragma once

#include "tools.hpp"

inline uint64_t kilobytes(uint32_t kb) {
    return(kb * 1024);
}

inline uint64_t megabytes(uint32_t mb) {
    return(kilobytes(mb * 1024));
}

struct free_arena_header_t {
    free_arena_header_t *next;
};

struct arena_allocator_t {
    uint32_t historically_allocated_arena_count;
    // Size of each arena
    uint32_t arena_size;
    // Size of entire pool
    uint32_t pool_size;
    // Pointer to beginning of pool
    void *pool;

    free_arena_header_t *head;

    void pool_init(
        uint32_t arena_size,
        uint32_t pool_size);
    
    void *allocate_arena();
    
    // Returns NULL
    void *free_arena(
        void *pointer);

    void free_pool();
};

struct linear_allocator_t {
    void *start;
    void *current;
    uint32_t max_size;

    void init(
        uint32_t max_size);
    
    void *allocate(
        uint32_t size);

    void clear();
};

void global_linear_allocator_init(
    uint32_t size);

void *linear_malloc(
    uint32_t size);

void linear_clear();

// Free list allocator
#define FL_MALLOC(type, n) (type *)malloc_debug(sizeof(type) * (n))
#define FL_FREE(ptr) free_debug(ptr)

template <typename T>
T *flmalloc(uint32_t count = 1) {
    return (T *)malloc_debug(sizeof(T) * count);
}

template <typename T>
void flfree(T *ptr) {
    free_debug(ptr);
}

// Linear allocator
#define LN_MALLOC(type, n) (type *)linear_malloc(sizeof(type) * (n))
#define LN_CLEAR() linear_clear()

template <typename T>
T *lnmalloc(uint32_t count = 1) {
    return (T *)linear_malloc(sizeof(T) * count);
}

template <typename T>
void lnclear(T *ptr) {
    linear_clear();
}
