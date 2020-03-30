#pragma once

#include "tools.hpp"

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
