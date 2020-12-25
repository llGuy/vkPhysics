#include "vkph_state.hpp"
#include "engine/vkph_map.hpp"
#include "vkph_chunk.hpp"

#include <common/string.hpp>
#include <common/tokeniser.hpp>
#include <common/serialiser.hpp>

namespace vkph {

static void s_nullify_player_memory(state_t *g) {
    for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
        g->players.data[i] = NULL;
    }
}

void state_t::prepare() {
    { // General
        current_tick = 0;
        load_map_names();
        game_mode = game_mode_t::INVALID;
    }

    { // Players
        players.init(PLAYER_MAX_COUNT);

        for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
            players.data[i] = NULL;
        }

        players.init(PLAYER_MAX_COUNT);
        s_nullify_player_memory(this);

        for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
            client_to_local_id_map[i] = -1;
        }
    }

    { // Chunks
        chunk_indices.init();
        chunks.init(CHUNK_MAX_LOADED_COUNT);

        max_modified_chunks = CHUNK_MAX_LOADED_COUNT / 2;
        modified_chunk_count = 0;
        modified_chunks = FL_MALLOC(chunk_t *, max_modified_chunks);

        flags.track_history = 1;
    }

    { // Projectiles
        rocks.init();
        predicted_hits.init(60);
    }

    { // Maps
        load_map_names();
    }
}

void state_t::configure_game_mode(game_mode_t mode) {
    game_mode = mode;
}

void state_t::configure_map(const char *map_path) {
    current_map_path = map_path;
}

void state_t::configure_team_count(uint32_t count) {
    team_count = count;
    teams = FL_MALLOC(team_t, team_count);
    memset(teams, 0, sizeof(team_t) * team_count);
    memset(team_color_to_index, 0, sizeof(uint32_t) * (uint32_t)team_color_t::COUNT);
}

void state_t::configure_team(
    uint32_t team_id,
    team_color_t color,
    uint32_t player_count) {
    team_t *t = &teams[team_id];
    t->init(color, player_count, 0);
    team_color_to_index[color] = team_id;
}

void state_t::start_session() {
    if (current_map_path)
        current_map_data = load_map(current_map_path);

    current_tick = 0;
}

void state_t::stop_session() {
    // Deinitialise everything in the game, unload map, etc...
    current_map_data = NULL;
    game_mode = game_mode_t::INVALID;
    team_count = 0;
    FL_FREE(teams);
}

void state_t::set_teams(
    uint32_t count,
    team_info_t *team_infos) {
    team_count = count;

    if (teams)
        FL_FREE(teams);

    teams = FL_MALLOC(team_t, team_count);

    for (uint32_t i = 0; i < team_count; ++i) {
        teams[i].init(
            team_infos[i].color,
            team_infos[i].max_players,
            team_infos[i].player_count);

        team_color_to_index[team_infos[i].color] = i;
    }
}

void state_t::change_player_team(player_t *player, team_color_t color) {
    if (player->flags.team_color != color) {
        if (color > team_color_t::INVALID && color < team_color_t::COUNT) {
            if (player->flags.team_color != team_color_t::INVALID) {
                // Remove player from the other team
                team_color_t previous_team = (team_color_t)player->flags.team_color;
                uint32_t prev_idx = team_color_to_index[previous_team];
                teams[prev_idx].remove_player(player->local_id);
            }

            uint32_t index = team_color_to_index[color];
            teams[index].add_player(player->local_id);
            player->flags.team_color = color;
        }
    }
}

void state_t::add_player_to_team(player_t *player, team_color_t color) {
    if (color > team_color_t::INVALID && color < team_color_t::COUNT) {
        uint32_t index = team_color_to_index[color];
        teams[index].add_player(player->local_id);
        player->flags.team_color = color;
    }
}

void state_t::remove_player_from_team(player_t *player) {
    if (player->flags.team_color != team_color_t::INVALID) {
        team_color_t previous_team = (team_color_t)player->flags.team_color;
        uint32_t prev_idx = team_color_to_index[previous_team];
        teams[prev_idx].remove_player(player->local_id);
    }
}

bool state_t::check_team_joinable(team_color_t color) {
    // Make sure that player count in team == min(player counts of teams)
    uint32_t min_player_count = teams[0].player_count();
    
    for (uint32_t i = 1; i < team_count; ++i) {
        min_player_count = MIN(min_player_count, teams[i].player_count());
    }

    uint32_t team_index = team_color_to_index[color];

    return (min_player_count == teams[team_index].player_count());
}

void state_t::timestep_begin(float dt_in) {
    dt = dt_in;
}

void state_t::timestep_end() {
    ++current_tick;
}

player_t *state_t::add_player() {
    uint32_t player_index = players.add();
    players[player_index] = FL_MALLOC(player_t, 1);
    player_t *player_ptr = players[player_index];
    memset(player_ptr, 0, sizeof(player_t));
    player_ptr->local_id = player_index;

    return player_ptr;
}

void state_t::remove_player(uint32_t local_id) {
    player_t *p = players[local_id];
    if (p) {
        if (p->render) {
            FL_FREE(p->render);
        }

        state_t::players[local_id] = NULL;
        // TODO: Free const char *name?
        players.remove(local_id);

        FL_FREE(p);
    }
}

void state_t::clear_players() {
    for (uint32_t i = 0; i < players.data_count; ++i) {
        remove_player(i);
    }

    players.clear();

    for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
        client_to_local_id_map[i] = -1;
    }
}

int32_t state_t::get_local_id(uint16_t client_id) {
    return client_to_local_id_map[client_id];
}

player_t *state_t::get_player(int32_t local_id) {
    if (local_id >= 0) {
        return players[local_id];
    }
    else {
        return NULL;
    }
}

void state_t::clear_chunks() {
    if (chunks.data_count) {
        chunks.clear();
        chunk_indices.clear();
    }
}

chunk_t *state_t::get_chunk(
    const ivector3_t &coord) {
    uint32_t hash = hash_chunk_coord(coord);
    uint32_t *index = chunk_indices.get(hash);
    
    if (index) {
        // Chunk was already added
        return chunks[*index];
    }
    else {
        uint32_t i = chunks.add();
        chunk_t *&chunk = chunks[i];
        chunk = FL_MALLOC(chunk_t, 1);
        chunk->init(i, coord);

        chunk_indices.insert(hash, i);

        return chunk;
    }
}

chunk_t *state_t::access_chunk(const ivector3_t &coord) {
    uint32_t hash = hash_chunk_coord(coord);
    uint32_t *index = chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return chunks[*index];
    }
    else {
        return NULL;
    }
}

const chunk_t *state_t::access_chunk(const ivector3_t &coord) const {
    uint32_t hash = hash_chunk_coord(coord);
    const uint32_t *index = chunk_indices.get(hash);

    if (index) {
        // Chunk was already added
        return chunks[*index];
    }
    else {
        return NULL;
    }
}

chunk_t **state_t::get_active_chunks(
    uint32_t *count) {
    *count = chunks.data_count;
    return chunks.data;
}

chunk_t **state_t::get_modified_chunks(
    uint32_t *count) {
    *count = modified_chunk_count;
    return modified_chunks;
}
 
void state_t::reset_modification_tracker() {
    for (uint32_t i = 0; i < modified_chunk_count; ++i) {
        chunk_t *c = modified_chunks[i];
        c->flags.made_modification = 0;

        for (int32_t v = 0; v < c->history.modification_count; ++v) {
            c->history.modification_pool[c->history.modification_stack[v]] = CHUNK_SPECIAL_VALUE;
        }

        c->history.modification_count = 0;
    }

    modified_chunk_count = 0;
}

map_t *state_t::load_map(const char *path) {
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

    current_map_path = path;
    current_map_data = current_loaded_map;

    return current_loaded_map;
}

void state_t::save_map(map_t *map) {
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
        ++saved_chunk_count;

        uint32_t before_chunk_ptr = serialiser.data_buffer_head;

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

                    if (zero_count == CHUNK_VOXEL_COUNT) {
                        serialiser.data_buffer_head = before_chunk_ptr;
                        --saved_chunk_count;
                        break;
                    }

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

    serialiser.serialise_uint32(saved_chunk_count, &serialiser.data_buffer[pointer_to_chunk_count]);

    LOG_INFOV("Saved map (%d chunks) - byte size: %d\n", saved_chunk_count, serialiser.data_buffer_head);
    write_file(map_file, serialiser.data_buffer, serialiser.data_buffer_head);

    free_file(map_file);
}

void state_t::add_map_name(const char *map_name, const char *path) {
    // If the map files hasn't been created, or if the name wasn't in the map_names file (or both), this function will be called
    // If it is the case that the name was in the map_names file, just create a new file, don't add it to the map names list
    // Otherwise, add it to the map name list
    uint32_t *item = map_name_to_index_.get(simple_string_hash(map_name));
    if (!item) {
        auto *p = &map_names.maps[map_names.count++];
        p->name = map_name;
        p->path = path;
    }
}

void state_t::load_map_names() {
    map_name_to_index_.init();

    map_names_file_ = create_file("assets/maps/map_names", FLF_TEXT);
    file_contents_t contents = read_file(map_names_file_);

    uint32_t byte_counter = 0;
    char *p = (char *)contents.data;

    char map_name[40] = {};
    char map_path[40] = {};

    map_names.count = 0;

    while (p && (p - (char *)contents.data) < contents.size && 
        map_names.count < MAX_MAP_COUNT) {
        { // Check if we hit the keyword end
            if (!memcmp(p, "end", 3)) {
                break;
            }
        }

        p = skip_to(p, '\"');

        char *end_of_name = skip_to(p, '\"');
        map_names.maps[map_names.count].name = create_fl_string(p, (end_of_name - p) - 1);
        map_name_to_index_.insert(simple_string_hash(map_names.maps[map_names.count].name), map_names.count);

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

}
