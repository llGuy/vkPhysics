#pragma once

#include <stdint.h>
#include <math.hpp>

namespace vkph {

/*
  This is for instance, when in the game menu, there needs to be a place from
  which to view the entire map.
 */
struct map_view_info_t {
    vector3_t pos, dir, up;
};

struct map_t {
    const char *path;
    bool is_new;

    // Stuff that is contained inside the file that path is pointing to
    const char *name;
    uint32_t chunk_count;

    map_view_info_t view_info;
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

}
