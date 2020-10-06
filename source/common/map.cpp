#include "map.hpp"
#include "common/chunk.hpp"
#include "files.hpp"
#include <string.h>
#include <stdio.h>
#include "string.hpp"
#include "constant.hpp"
#include "serialiser.hpp"

static file_handle_t map_names_file;
static map_names_t map_names;

void load_map_names() {
    map_names_file = create_file("assets/maps/map_names", FLF_TEXT);
    file_contents_t contents = read_file(map_names_file);

    char *p = (char *)contents.data;

    char map_name[40] = {};
    char map_path[40] = {};

    map_names.count = 0;

    while (p && map_names.count < MAX_MAP_COUNT) {
        p = skip_to(p, '\"');

        char *end_of_name = skip_to(p, '\"');
        map_names.maps[map_names.count].name = create_fl_string(p, (end_of_name - p) - 1);

        p = skip_while(end_of_name, ' ');

        char *end_of_path = skip_to(p, '\n');
        if (!end_of_path) {
            end_of_path = skip_to(p, 0);
            map_names.maps[map_names.count].path = create_fl_string(p, (end_of_path - p));
        }
        else {
            map_names.maps[map_names.count].path = create_fl_string(p, (end_of_path - p) - 1);
        }

        p = end_of_path;

        map_names.count++;

        memset(map_name, 0, sizeof(map_name));
        memset(map_path, 0, sizeof(map_path));

        if (p[1] == 0) {
            break;
        }
    }
}

map_names_t *get_map_names() {
    return &map_names;
}

map_t *load_map(const char *path) {
    map_t *current_loaded_map = FL_MALLOC(map_t, 1);

    current_loaded_map->path = path;

    char full_path[50] = {};
    sprintf(full_path, "assets/maps/%s", path);

    file_handle_t map_file = create_file(full_path, FLF_BINARY);
    if (does_file_exist(map_file)) {
        file_contents_t contents = read_file(map_file);

        // Deserialise stuff
        serialiser_t serialiser = {};
        serialiser.data_buffer = contents.data;
        serialiser.data_buffer_head = 0;
        serialiser.data_buffer_size = contents.size;

        current_loaded_map->name = serialiser.deserialise_fl_string();
        current_loaded_map->chunk_count = serialiser.deserialise_uint32();

        for (uint32_t i = 0; i < current_loaded_map->chunk_count; ++i) {
            int16_t x = serialiser.deserialise_int16();
            int16_t y = serialiser.deserialise_int16();
            int16_t z = serialiser.deserialise_int16();

            chunk_t *chunk = get_chunk(ivector3_t(x, y, z));
            chunk->flags.has_to_update_vertices = 1;

            // Also force update surrounding chunks
            get_chunk(ivector3_t(x + 1, y, z))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x - 1, y, z))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x, y + 1, z))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x, y - 1, z))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x, y, z + 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x, y, z - 1))->flags.has_to_update_vertices = 1;
        
            get_chunk(ivector3_t(x + 1, y + 1, z + 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x + 1, y + 1, z - 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x + 1, y - 1, z + 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x + 1, y - 1, z - 1))->flags.has_to_update_vertices = 1;

            get_chunk(ivector3_t(x - 1, y + 1, z + 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x - 1, y + 1, z - 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x - 1, y - 1, z + 1))->flags.has_to_update_vertices = 1;
            get_chunk(ivector3_t(x - 1, y - 1, z - 1))->flags.has_to_update_vertices = 1;

            for (uint32_t v = 0; v < CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH * CHUNK_EDGE_LENGTH;) {
                uint8_t current_value = serialiser.deserialise_uint8();

                if (current_value == CHUNK_SPECIAL_VALUE) {
                    chunk->voxels[v] = 0;
                    ++v;

                    // Repeating zeros
                    uint32_t zero_count = serialiser.deserialise_uint32();
                    chunk->voxels[v + 1] = 0;
                    chunk->voxels[v + 2] = 0;
                    chunk->voxels[v + 3] = 0;
                    chunk->voxels[v + 4] = 0;

                    v += 4;

                    uint32_t previous_v = v;
                    for (; v < previous_v + zero_count - 5; ++v) {
                        chunk->voxels[v] = 0;
                    }
                }
                else {
                    chunk->voxels[v] = current_value;
                    ++v;
                }
            }
        }

        current_loaded_map->is_new = 0;
    }
    else {
        current_loaded_map->is_new = 1;
    }

    return current_loaded_map;
}

void save_map(map_t *map) {
    // Serialise stuff
}

void unload_map(map_t *map) {
    memset(map, 0, sizeof(map_t));
    // TODO: Make sure to reset all the voxels from the chunks
}

// Saving the file will happen later
void add_map_name(const char *map_name, const char *path) {
    auto *p = &map_names.maps[map_names.count++];
    p->name = map_name;
    p->path = path;
}
