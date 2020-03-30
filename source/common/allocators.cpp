#include "allocators.hpp"

void arena_allocator_t::pool_init(
    uint32_t asize,
    uint32_t psize) {
    arena_size = asize;
    pool_size = psize;
    pool = malloc(pool_size);

    head = (free_arena_header_t *)pool;
    head->next = NULL;
}

void *arena_allocator_t::allocate_arena() {
    // Go to head first
    free_arena_header_t *header = head;
    void *p = (void *)header;

    if (header->next) {
        head = header->next;
    }
    else {
        uint8_t *current = (uint8_t *)header;

        current += arena_size;

        head = (free_arena_header_t *)current;
    }

    return p;
}

void *arena_allocator_t::free_arena(
    void *pointer) {
    free_arena_header_t *new_header = (free_arena_header_t *)pointer;

    if (pointer < head) {
        new_header->next = head;
        head = new_header;

        return NULL;
    }
    else {
        free_arena_header_t *current = head;

        for (; current->next; current = current->next) {
            if (pointer < current->next) {
                new_header->next = current->next;
                current->next = new_header;

                return NULL;
            }
        }

        new_header->next = NULL;
        current->next = new_header;

        return NULL;
    }
}

void arena_allocator_t::free_pool() {
    free(pool);
}
