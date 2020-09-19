#pragma once

#include <stdint.h>

struct map_t {
    const char *path;
    bool is_new;

    // Stuff that is contained inside the file that path is pointing to
    const char *name;
    uint32_t chunk_count;
};

#define MAX_MAP_COUNT 10

// At the beginning of the game, the maps file gets loaded,
// in which the names of each map is loaded
struct map_names_t {
    uint32_t count;

    struct pair_t {
        const char *name;
        const char *path;
    } maps[MAX_MAP_COUNT];
};

void load_map_names();
map_names_t *get_map_names();
map_t *load_map(const char *path);
void save_map(map_t *map);
void unload_map(map_t *map);
