#include "map.hpp"
#include "common/allocators.hpp"
#include "common/chunk.hpp"
#include "common/containers.hpp"
#include "files.hpp"
#include <bits/stdint-uintn.h>
#include <string.h>
#include <stdio.h>
#include "string.hpp"
#include "constant.hpp"
#include "serialiser.hpp"

static file_handle_t map_names_file;
static map_names_t map_names;
static hash_table_t<uint32_t, 10, 5, 5> map_name_to_index;

void load_map_names() {
    map_name_to_index.init();

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
        map_name_to_index.insert(simple_string_hash(map_names.maps[map_names.count].name), map_names.count);

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
                uint8_t current_color = serialiser.deserialise_uint8();

                if (current_value == CHUNK_SPECIAL_VALUE) {
                    chunk->voxels[v].value = 0;
                    chunk->voxels[v].color = 0;
                    ++v;

                    // Repeating zeros
                    uint32_t zero_count = serialiser.deserialise_uint32();
                    chunk->voxels[v + 1].value = 0;
                    chunk->voxels[v + 1].color = 0;
                    chunk->voxels[v + 2].value = 0;
                    chunk->voxels[v + 2].color = 0;

                    v += 2;

                    uint32_t previous_v = v;
                    for (; v < previous_v + zero_count - 3; ++v) {
                        chunk->voxels[v].value = 0;
                        chunk->voxels[v].color = 0;
                    }
                }
                else {
                    chunk->voxels[v].value = current_value;
                    chunk->voxels[v].color = current_color;
                    ++v;
                }
            }
        }

        current_loaded_map->is_new = 0;
    }
    else {
        current_loaded_map->is_new = 1;
    }

    free_file(map_file);

    return current_loaded_map;
}

void save_map(map_t *map) {
    // Serialise stuff
    char full_path[50] = {};
    sprintf(full_path, "assets/maps/%s", map->path);
    file_handle_t map_file = create_file(full_path, FLF_BINARY | FLF_WRITEABLE | FLF_OVERWRITE);

    serialiser_t serialiser = {};
    serialiser.data_buffer = LN_MALLOC(uint8_t, 1024 * 128);
    serialiser.data_buffer_head = 0;
    serialiser.data_buffer_size = 1024 * 128;

    uint32_t chunk_count = 0;
    chunk_t **chunks = get_active_chunks(&chunk_count);

    serialiser.serialise_string(map->name);
    uint32_t pointer_to_chunk_count = serialiser.data_buffer_head;
    serialiser.serialise_uint32(0);

    uint32_t saved_chunk_count = 0;
    for (uint32_t i = 0; i < chunk_count; ++i) {
        if (chunks[i]->flags.active_vertices) {
            ++saved_chunk_count;

            serialiser.serialise_int16(chunks[i]->chunk_coord.x);
            serialiser.serialise_int16(chunks[i]->chunk_coord.y);
            serialiser.serialise_int16(chunks[i]->chunk_coord.z);

            for (uint32_t v_index = 0; v_index < CHUNK_VOXEL_COUNT; ++v_index) {
                voxel_t current_voxel = chunks[i]->voxels[v_index];
                if (current_voxel.value == 0) {
                    uint32_t before_head = serialiser.data_buffer_head;

                    static constexpr uint32_t MAX_ZERO_COUNT_BEFORE_COMPRESSION = 3;

                    uint32_t zero_count = 0;
                    for (; chunks[i]->voxels[v_index].value == 0 && zero_count < MAX_ZERO_COUNT_BEFORE_COMPRESSION; ++v_index, ++zero_count) {
                        serialiser.serialise_uint8(0);
                        serialiser.serialise_uint8(0);
                    }

                    if (zero_count == MAX_ZERO_COUNT_BEFORE_COMPRESSION) {
                        for (; chunks[i]->voxels[v_index].value == 0; ++v_index, ++zero_count) {}

                        serialiser.data_buffer_head = before_head;
                        serialiser.serialise_uint8(CHUNK_SPECIAL_VALUE);
                        serialiser.serialise_uint8(CHUNK_SPECIAL_VALUE);
                        serialiser.serialise_uint32(zero_count);
                    }

                    v_index -= 1;
                }
                else {
                    serialiser.serialise_uint8(current_voxel.value);
                    serialiser.serialise_uint8(current_voxel.color);
                }
            }
        }
    }

    serialiser.serialise_uint32(saved_chunk_count, &serialiser.data_buffer[pointer_to_chunk_count]);

    write_file(map_file, serialiser.data_buffer, serialiser.data_buffer_head);

    free_file(map_file);
}

void unload_map(map_t *map) {
    memset(map, 0, sizeof(map_t));
    // TODO: Make sure to reset all the voxels from the chunks
}

// Saving the file will happen later
void add_map_name(const char *map_name, const char *path) {
    // If the map files hasn't been created, or if the name wasn't in the map_names file (or both), this function will be called
    // If it is the case that the name was in the map_names file, just create a new file, don't add it to the map names list
    // Otherwise, add it to the map name list
    uint32_t *item = map_name_to_index.get(simple_string_hash(map_name));
    if (!item) {
        auto *p = &map_names.maps[map_names.count++];
        p->name = map_name;
        p->path = path;
    }
}
