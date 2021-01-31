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

    void pool_init(uint32_t arena_size, uint32_t pool_size);
    void *allocate_arena();
    // Returns NULL
    void *free_arena(void *pointer);
    void free_pool();
};

struct linear_allocator_t {
    void *start;
    void *current;
    uint32_t max_size;

    void init(uint32_t max_size);
    void *allocate(uint32_t size);
    void clear();
};

void global_linear_allocator_init(uint32_t size);
void *linear_malloc(uint32_t size);
void linear_clear();

// Implement custom free list allocator
template <typename T>
inline T *flmalloc(uint32_t count = 1) {
    return (T *)malloc_debug(sizeof(T) * count);
}

template <typename T, typename ...Constr>
inline T *flmalloc_and_init(Constr &&...args) {
    T *p = (T *)malloc_debug(sizeof(T));
    new(p) T(std::forward<Constr>(args)...);
    return p;
}

template <typename T>
inline void flfree(T *ptr) {
    free_debug(ptr);
}

template <typename T>
inline T *lnmalloc(uint32_t count = 1) {
    return (T *)linear_malloc(sizeof(T) * count);
}

inline void lnclear() {
    linear_clear();
}

extern linear_allocator_t g_linear_allocator;
