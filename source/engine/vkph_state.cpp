#include "vkph_state.hpp"
#include "vkph_constant.hpp"
#include "vkph_map.hpp"
#include "vkph_team.hpp"
#include "vkph_terraform.hpp"
#include "vkph_chunk.hpp"

#include <string.hpp>
#include <tokeniser.hpp>
#include <serialiser.hpp>

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
        current_map_data = *load_map(current_map_path);

    current_tick = 0;
}

void state_t::stop_session() {
    // Deinitialise everything in the game, unload map, etc...
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

            player->terraform_package.color = team_color_to_voxel_color(color);
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

bool state_t::check_team_joinable(team_color_t color) const {
    // Make sure that player count in team == min(player counts of teams)
    uint32_t min_player_count = teams[0].player_count();
    
    for (uint32_t i = 1; i < team_count; ++i) {
        min_player_count = MIN(min_player_count, teams[i].player_count());
    }

    uint32_t team_index = team_color_to_index[color];

    return (min_player_count == teams[team_index].player_count());
}

void state_t::timestep_begin(float dt_in) {
    delta_time = dt_in;
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

int32_t state_t::get_local_id(uint16_t client_id) const {
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

const player_t *state_t::get_player(int32_t local_id) const {
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

chunk_t **state_t::get_active_chunks(uint32_t *count) {
    *count = chunks.data_count;
    return chunks.data;
}

const chunk_t **state_t::get_active_chunks(uint32_t *count) const {
    *count = chunks.data_count;
    return (const chunk_t **)chunks.data;
}

chunk_t **state_t::get_modified_chunks(uint32_t *count) {
    *count = modified_chunk_count;
    return modified_chunks;
}

const chunk_t **state_t::get_modified_chunks(uint32_t *count) const {
    *count = modified_chunk_count;
    return (const chunk_t **)modified_chunks;
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

void state_t::flag_modified_chunks(net::chunk_modifications_t *modifications, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        net::chunk_modifications_t *m_ptr = &modifications[i];
        vkph::chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 1;
        c_ptr->flags.index_of_modification_struct = i;
    }
}

void state_t::unflag_modified_chunks(net::chunk_modifications_t *modifications, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        net::chunk_modifications_t *m_ptr = &modifications[i];
        vkph::chunk_t *c_ptr = get_chunk(ivector3_t(m_ptr->x, m_ptr->y, m_ptr->z));
        c_ptr->flags.modified_marker = 0;
        c_ptr->flags.index_of_modification_struct = 0;
    }
}

map_t *state_t::load_map(const char *path) {
    current_map_data.path = path;

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

        current_map_data.name = serialiser.deserialise_fl_string();

        current_map_data.view_info.pos = serialiser.deserialise_vector3();
        current_map_data.view_info.dir = serialiser.deserialise_vector3();
        current_map_data.view_info.up = serialiser.deserialise_vector3();

        current_map_data.chunk_count = serialiser.deserialise_uint32();

        for (uint32_t i = 0; i < current_map_data.chunk_count; ++i) {
            int16_t x = serialiser.deserialise_int16();
            int16_t y = serialiser.deserialise_int16();
            int16_t z = serialiser.deserialise_int16();

            chunk_t *chunk = get_chunk(ivector3_t(x, y, z));
            chunk->flags.has_to_update_vertices = 1;

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

        current_map_data.is_new = 0;
    }
    else {
        current_map_data.is_new = 1;
    }

    free_file(map_file);

    current_map_path = path;

    return &current_map_data;
}

void state_t::save_map(map_t *map) {
    if (!map) {
        map = &current_map_data;
    }

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
    serialiser.serialise_vector3(map->view_info.pos);
    serialiser.serialise_vector3(map->view_info.dir);
    serialiser.serialise_vector3(map->view_info.up);

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

                    if (zero_count >= CHUNK_VOXEL_COUNT) {
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

template <typename T>
static void s_iterate_3d(const ivector3_t &center, uint32_t radius, T to_apply) {
    int32_t start_z = center.z - (int32_t)radius;
    int32_t start_y = center.y - (int32_t)radius;
    int32_t start_x = center.x - (int32_t)radius;

    int32_t diameter = (int32_t)radius * 2 + 1;

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                to_apply(x, y, z);
            }
        }
    }
}

void state_t::generate_hollow_sphere(sphere_create_info_t *info) {
    float (* generation_proc)(float distance_squared, float radius_squared);
    switch(info->type) {
    case GT_ADDITIVE: {
        generation_proc =
            [] (float dsqu, float rsqu) {
                float diff = abs(dsqu - rsqu);
                if (diff < 120.0f)
                    return 1.0f - (diff / 120.0f);
                else
                    return 0.0f;
            };
    } break;

    case GT_DESTRUCTIVE: generation_proc = [] (float dsqu, float rsqu) {return 0.0f;}; break;
    }

    ivector3_t vs_center = space_world_to_voxel(info->ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = space_voxel_to_chunk(vs_center);

    chunk_t *current_chunk = get_chunk(
        current_chunk_coord);

    current_chunk->flags.has_to_update_vertices = 1;

    int32_t diameter = (int32_t)info->ws_radius * 2 + 1;

    int32_t start_z = vs_center.z - (int32_t)info->ws_radius;
    int32_t start_y = vs_center.y - (int32_t)info->ws_radius;
    int32_t start_x = vs_center.x - (int32_t)info->ws_radius;

    float radius_squared = info->ws_radius * info->ws_radius;
    float smaller_radius_squared = (info->ws_radius - 10) * (info->ws_radius - 10);

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                ivector3_t vs_position = ivector3_t(x, y, z);
                vector3_t vs_float = vector3_t((float)x, (float)y, (float)z);
                vector3_t vs_diff_float = vs_float - vs_float_center;

                float distance_squared = glm::dot(vs_diff_float, vs_diff_float);

                if (distance_squared <= radius_squared) {
                    ivector3_t c = space_voxel_to_chunk(vs_position);

                    ivector3_t chunk_origin_diff = vs_position - current_chunk_coord * (int32_t)CHUNK_EDGE_LENGTH;
                    if (chunk_origin_diff.x >= 0 && chunk_origin_diff.x < 16 &&
                        chunk_origin_diff.y >= 0 && chunk_origin_diff.y < 16 &&
                        chunk_origin_diff.z >= 0 && chunk_origin_diff.z < 16) {
                        // Is within current chunk boundaries
                        float proportion = generation_proc(distance_squared, smaller_radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * info->max_value);
                        if (v->value < new_value) {
                            v->value = new_value;
                            v->color = info->color;
                        }
                    }
                    else {
                        ivector3_t c = space_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = get_chunk(c);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = generation_proc(distance_squared, smaller_radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * info->max_value);
                        if (v->value < new_value) {
                            v->value = new_value;
                            v->color = info->color;
                        }
                    }
                }
            }
        }
    }
}

void state_t::generate_sphere(sphere_create_info_t *info) {
    float (* generation_proc)(float distance_squared, float radius_squared);
    switch(info->type) {
    case GT_ADDITIVE: generation_proc = [] (float dsqu, float rsqu) {return 1.0f - (dsqu / rsqu);}; break;
    case GT_DESTRUCTIVE: generation_proc = [] (float dsqu, float rsqu) {return 0.0f;}; break;
    }

    ivector3_t vs_center = space_world_to_voxel(info->ws_center);
    vector3_t vs_float_center = (vector3_t)(vs_center);

    ivector3_t current_chunk_coord = space_voxel_to_chunk(vs_center);

    chunk_t *current_chunk = get_chunk(
        current_chunk_coord);

    current_chunk->flags.has_to_update_vertices = 1;

    int32_t diameter = (int32_t)info->ws_radius * 2 + 1;

    int32_t start_z = vs_center.z - (int32_t)info->ws_radius;
    int32_t start_y = vs_center.y - (int32_t)info->ws_radius;
    int32_t start_x = vs_center.x - (int32_t)info->ws_radius;

    float radius_squared = info->ws_radius * info->ws_radius;

    for (int32_t z = start_z; z < start_z + diameter; ++z) {
        for (int32_t y = start_y; y < start_y + diameter; ++y) {
            for (int32_t x = start_x; x < start_x + diameter; ++x) {
                ivector3_t vs_position = ivector3_t(x, y, z);
                vector3_t vs_float = vector3_t((float)x, (float)y, (float)z);
                vector3_t vs_diff_float = vs_float - vs_float_center;

                float distance_squared = glm::dot(vs_diff_float, vs_diff_float);

                if (distance_squared <= radius_squared) {
                    ivector3_t c = space_voxel_to_chunk(vs_position);

                    ivector3_t chunk_origin_diff = vs_position - current_chunk_coord * (int32_t)CHUNK_EDGE_LENGTH;
                    //if (c.x == current_chunk_coord.x && c.y == current_chunk_coord.y && c.z == current_chunk_coord.z) {
                    if (chunk_origin_diff.x >= 0 && chunk_origin_diff.x < 16 &&
                        chunk_origin_diff.y >= 0 && chunk_origin_diff.y < 16 &&
                        chunk_origin_diff.z >= 0 && chunk_origin_diff.z < 16) {
                        // Is within current chunk boundaries
                        float proportion = generation_proc(distance_squared, radius_squared);

                        ivector3_t voxel_coord = chunk_origin_diff;

                        //current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)] = (uint32_t)((proportion) * (float)MAX_VOXEL_VALUE_I);

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * info->max_value);
                        v->value = new_value;
                        v->color = info->color;
                    }
                    else {
                        ivector3_t c = space_voxel_to_chunk(vs_position);

                        // In another chunk, need to switch current_chunk pointer
                        current_chunk = get_chunk(c);
                        current_chunk_coord = c;

                        current_chunk->flags.has_to_update_vertices = 1;

                        float proportion = generation_proc(distance_squared, radius_squared);

                        ivector3_t voxel_coord = vs_position - current_chunk_coord * CHUNK_EDGE_LENGTH;

                        voxel_t *v = &current_chunk->voxels[get_voxel_index(voxel_coord.x, voxel_coord.y, voxel_coord.z)];
                        uint8_t new_value = (uint32_t)((proportion) * info->max_value);
                        v->value = new_value;
                        v->color = info->color;
                    }
                }
            }
        }
    }
}

void state_t::generate_platform(platform_create_info_t *info) {
    uint8_t (* generation_proc)();
    switch (info->type) {
    case GT_ADDITIVE: generation_proc = [] () -> uint8_t {return 80;}; break;
    case GT_DESTRUCTIVE: generation_proc = [] () -> uint8_t {return 0;}; break;
    }

    ivector3_t centeriv3 = (ivector3_t)(info->position);

    for (int32_t z = centeriv3.z - (int32_t)info->depth / 2; z < centeriv3.z + (int32_t)info->depth / 2; ++z) {
        for (int32_t x = centeriv3.x - (int32_t)info->width / 2; x < centeriv3.x + (int32_t)info->width / 2; ++x) {
            ivector3_t voxel_coord = ivector3_t((float)x, -2.0f, (float)z);
            ivector3_t chunk_coord = space_voxel_to_chunk(voxel_coord);
            chunk_t *chunk = get_chunk(chunk_coord);
            chunk->flags.has_to_update_vertices = 1;
            ivector3_t local_coord = space_voxel_to_local_chunk(voxel_coord);
            uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
            chunk->voxels[index].value = generation_proc();
            chunk->voxels[index].color = info->color;
        }
    }
}

void state_t::generate_math_equation(math_equation_create_info_t *info) {
    uint8_t (* generation_proc)(float equation_result);
    switch (info->type) {
    case GT_ADDITIVE: generation_proc = [] (float equation_result) {return (uint8_t)(150.0f * equation_result);}; break;
    case GT_DESTRUCTIVE: generation_proc = [] (float equation_result) {return (uint8_t)0;}; break;
    }

    ivector3_t centeriv3 = (ivector3_t)(info->ws_center);
    ivector3_t extentiv3 = (ivector3_t)(info->ws_extent);

    for (int32_t z = centeriv3.z - extentiv3.z / 2; z < centeriv3.z + extentiv3.z / 2; ++z) {
        for (int32_t y = centeriv3.y - extentiv3.y / 2; y < centeriv3.y + extentiv3.y / 2; ++y) {
            for (int32_t x = centeriv3.x - extentiv3.x / 2; x < centeriv3.x + extentiv3.x / 2; ++x) {
                float c = info->equation((float)(x - centeriv3.x), (float)(y - centeriv3.y), (float)(z - centeriv3.z));

                if (c > 0.0f) {
                    ivector3_t voxel_coord = ivector3_t((float)x, (float)y, (float)z);
                    ivector3_t chunk_coord = space_voxel_to_chunk(voxel_coord);
                    chunk_t *chunk = get_chunk(chunk_coord);
                    chunk->flags.has_to_update_vertices = 1;
                    ivector3_t local_coord = space_voxel_to_local_chunk(voxel_coord);
                    uint32_t index = get_voxel_index(local_coord.x, local_coord.y, local_coord.z);
                    chunk->voxels[index].value = generation_proc(c);
                    chunk->voxels[index].color = info->color;
                }
            }
        }
    }
}

bool state_t::terraform_with_history(terraform_info_t *info) {
    if (info->package->ray_hit_terrain) {
        ivector3_t voxel = space_world_to_voxel(info->package->ws_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (!chunk->flags.made_modification) {
            // Push this chunk onto list of modified chunks
            modified_chunks[modified_chunk_count++] = chunk;
        }
                    
        chunk->flags.made_modification = 1;
        chunk->flags.has_to_update_vertices = 1;

        float coeff = 0.0f;
        switch(info->type) {
        case TT_DESTROY: {coeff = -1.0f;} break;
        case TT_BUILD: {coeff = +1.0f;} break;
        }

        float radius_squared = info->radius * info->radius;
        ivector3_t bottom_corner = voxel - ivector3_t((int32_t)info->radius);
        int32_t diameter = (int32_t)info->radius * 2 + 1;

        for (int32_t z = bottom_corner.z; z < bottom_corner.z + diameter; ++z) {
            for (int32_t y = bottom_corner.y; y < bottom_corner.y + diameter; ++y) {
                for (int32_t x = bottom_corner.x; x < bottom_corner.x + diameter; ++x) {
                    vector3_t current_voxel = vector3_t((float)x, (float)y, (float)z);
                    vector3_t diff = current_voxel - (vector3_t)voxel;
                    float distance_squared = glm::dot(diff, diff);

                    if (distance_squared <= radius_squared) {
                        ivector3_t current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                
                        if (current_local_coord.x < 0 || current_local_coord.x >= 16 ||
                            current_local_coord.y < 0 || current_local_coord.y >= 16 ||
                            current_local_coord.z < 0 || current_local_coord.z >= 16) {
                            // If the current voxel coord is out of bounds, switch chunks
                            ivector3_t chunk_coord = space_voxel_to_chunk(current_voxel);
                            chunk_t *new_chunk = get_chunk(chunk_coord);

                            chunk = new_chunk;

                            if (!chunk->flags.made_modification) {
                                // Push this chunk onto list of modified chunks
                                modified_chunks[modified_chunk_count++] = chunk;
                            }
                                        
                            chunk->flags.made_modification = 1;
                            chunk->flags.has_to_update_vertices = 1;

                            current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                        }

                        uint32_t voxel_index = get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z);
                        voxel_t *voxel = &chunk->voxels[voxel_index];
                        uint8_t voxel_value = voxel->value;
                        float proportion = 1.0f - (distance_squared / radius_squared);

                        int32_t current_voxel_value = (int32_t)voxel->value;

                        int32_t new_value = (int32_t)(proportion * coeff * info->dt * info->speed) + current_voxel_value;

                        uint8_t *vh = &chunk->history.modification_pool[voxel_index];
                                    
                        if (new_value > (int32_t)CHUNK_MAX_VOXEL_VALUE_I) {
                            voxel_value = (int32_t)CHUNK_MAX_VOXEL_VALUE_I;
                        }
                        else if (new_value < 0) {
                            voxel_value = 0;
                        }
                        else {
                            voxel_value = (uint8_t)new_value;
                        }

                        // Didn't add to the history yet
                        if (*vh == CHUNK_SPECIAL_VALUE && voxel_value != voxel->value) {
                            *vh = voxel->value;
                            chunk->history.modification_stack[chunk->history.modification_count++] = voxel_index;
                        }
                                    
                        voxel->value = voxel_value;
                        voxel->color = info->package->color;
                    }
                }
            }
        }

        return 1;
    }

    return 0;
}

bool state_t::terraform_without_history(terraform_info_t *info) {
    if (info->package->ray_hit_terrain) {
        ivector3_t voxel = space_world_to_voxel(info->package->ws_position);
        ivector3_t chunk_coord = space_voxel_to_chunk(voxel);
        chunk_t *chunk = access_chunk(chunk_coord);

        if (chunk) {
            ivector3_t local_voxel_coord = space_voxel_to_local_chunk(voxel);
            voxel_t *voxel_ptr = &chunk->voxels[get_voxel_index(local_voxel_coord.x, local_voxel_coord.y, local_voxel_coord.z)];
            if (voxel_ptr->value > CHUNK_SURFACE_LEVEL) {
                info->package->ray_hit_terrain = 1;

                chunk->flags.made_modification = 1;
                chunk->flags.has_to_update_vertices = 1;

                float coeff = 0.0f;
                switch(info->type) {
                        
                case TT_DESTROY: {
                    coeff = -1.0f;
                } break;
                        
                case TT_BUILD: {
                    coeff = +1.0f;
                } break;
                        
                }

                float radius_squared = info->radius * info->radius;
                ivector3_t bottom_corner = voxel - ivector3_t((int32_t)info->radius);
                int32_t diameter = (int32_t)info->radius * 2 + 1;

                for (int32_t z = bottom_corner.z; z < bottom_corner.z + diameter; ++z) {
                    for (int32_t y = bottom_corner.y; y < bottom_corner.y + diameter; ++y) {
                        for (int32_t x = bottom_corner.x; x < bottom_corner.x + diameter; ++x) {
                            vector3_t current_voxel = vector3_t((float)x, (float)y, (float)z);
                            vector3_t diff = current_voxel - (vector3_t)voxel;
                            float distance_squared = glm::dot(diff, diff);

                            if (distance_squared <= radius_squared) {
                                ivector3_t current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                
                                if (current_local_coord.x < 0 || current_local_coord.x >= 16 ||
                                    current_local_coord.y < 0 || current_local_coord.y >= 16 ||
                                    current_local_coord.z < 0 || current_local_coord.z >= 16) {
                                    // If the current voxel coord is out of bounds, switch chunks
                                    ivector3_t chunk_coord = space_voxel_to_chunk(current_voxel);
                                    chunk_t *new_chunk = get_chunk(chunk_coord);

                                    chunk = new_chunk;
                                    chunk->flags.made_modification = 1;
                                    chunk->flags.has_to_update_vertices = 1;

                                    current_local_coord = (ivector3_t)current_voxel - chunk->xs_bottom_corner;
                                }

                                uint32_t voxel_index = get_voxel_index(current_local_coord.x, current_local_coord.y, current_local_coord.z);

                                voxel_t *voxel = &chunk->voxels[voxel_index];
                                float proportion = 1.0f - (distance_squared / radius_squared);

                                int32_t current_voxel_value = (int32_t)voxel->value;

                                int32_t new_value = (int32_t)(proportion * coeff * info->dt * info->speed) + current_voxel_value;

                                uint8_t voxel_value = 0;
                                    
                                if (new_value > (int32_t)CHUNK_MAX_VOXEL_VALUE_I) {
                                    voxel_value = (int32_t)CHUNK_MAX_VOXEL_VALUE_I;
                                }
                                else if (new_value < 0) {
                                    voxel_value = 0;
                                }
                                else {
                                    voxel_value = (uint8_t)new_value;
                                }

                                voxel->value = voxel_value;
                                voxel->color = info->package->color;
                            }
                        }
                    }
                }
            }
        }

        return 1;
    }

    return 0;
}

bool state_t::terraform(terraform_info_t *info) {
    if (flags.track_history)
        return terraform_with_history(info);
    else
        return terraform_without_history(info);
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
        map_names.maps[map_names.count].name = create_fl_string(p, (uint32_t)(end_of_name - p) - 1);
        map_name_to_index_.insert(simple_string_hash(map_names.maps[map_names.count].name), map_names.count);

        p = skip_while(end_of_name, ' ');

        char *end_of_path = skip_to(p, '\n');
        if (!end_of_path) {
            end_of_path = skip_to(p, 0);
            map_names.maps[map_names.count].path = create_fl_string(p, (uint32_t)(end_of_path - p));
        }
        else {
            map_names.maps[map_names.count].path = create_fl_string(p, (uint32_t)(end_of_path - p) - 1);
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
