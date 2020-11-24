#include "common/constant.hpp"
#include "common/containers.hpp"
#include "map.hpp"
#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include <math.h>

game_t *g_game;

void game_allocate() {
    g_game = FL_MALLOC(game_t, 1);
}

static void s_nullify_player_memory(game_t *g) {
    for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
        g->players.data[i] = NULL;
    }
}

void game_t::init_memory() {
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
        local_rocks.init(PROJECTILE_MAX_LOCAL_ROCK_COUNT);
        remote_rocks.init(PROJECTILE_MAX_REMOTE_ROCK_COUNT);

        newly_spawned_rock_count = 0;
        for (uint32_t i = 0; i < 50; ++i) {
            newly_spawned_rocks[i] = 0;
        }
    }
}

void game_t::configure_game_mode(game_mode_t mode) {
    game_mode = mode;
}

void game_t::configure_map(const char *map_path) {
    current_map_path = map_path;
}

void game_t::configure_team_count(uint32_t count) {
    team_count = count;
    teams = FL_MALLOC(team_t, team_count);
    memset(teams, 0, sizeof(team_t) * team_count);
    memset(team_color_to_index, 0, sizeof(uint32_t) * (uint32_t)team_color_t::COUNT);
}

void game_t::configure_team(
    uint32_t team_id,
    team_color_t color,
    uint32_t player_count) {
    team_t *t = &teams[team_id];
    t->init(color, player_count, 0);
    team_color_to_index[color] = team_id;
}

void game_t::start_session() {
    if (current_map_path)
        current_map_data = load_map(current_map_path);

    current_tick = 0;
}

void game_t::stop_session() {
    // Deinitialise everything in the game, unload map, etc...
    unload_map(current_map_data);
    current_map_data = NULL;
    game_mode = game_mode_t::INVALID;
    team_count = 0;
    FL_FREE(teams);
}

void game_t::set_teams(
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

void game_t::change_player_team(player_t *player, team_color_t color) {
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

void game_t::add_player_to_team(player_t *player, team_color_t color) {
    if (color > team_color_t::INVALID && color < team_color_t::COUNT) {
        uint32_t index = team_color_to_index[color];
        teams[index].add_player(player->local_id);
        player->flags.team_color = color;
    }
}

void game_t::remove_player_from_team(player_t *player) {
    if (player->flags.team_color != team_color_t::INVALID) {
        team_color_t previous_team = (team_color_t)player->flags.team_color;
        uint32_t prev_idx = team_color_to_index[previous_team];
        teams[prev_idx].remove_player(player->local_id);
    }
}

bool game_t::check_team_joinable(team_color_t color) {
    // Make sure that player count in team == min(player counts of teams)
    uint32_t min_player_count = teams[0].player_count();
    
    for (uint32_t i = 1; i < team_count; ++i) {
        min_player_count = MIN(min_player_count, teams[i].player_count());
    }

    uint32_t team_index = team_color_to_index[color];

    return (min_player_count == teams[team_index].player_count());
}

void game_t::timestep_begin(float dt_in) {
    dt = dt_in;
}

void game_t::timestep_end() {
    ++current_tick;
}

player_t *game_t::add_player() {
    uint32_t player_index = players.add();
    players[player_index] = FL_MALLOC(player_t, 1);
    player_t *player_ptr = players[player_index];
    memset(player_ptr, 0, sizeof(player_t));
    player_ptr->local_id = player_index;

    return player_ptr;
}

void game_t::remove_player(uint32_t local_id) {
    player_t *p = players[local_id];
    if (p) {
        if (p->render) {
            FL_FREE(p->render);
        }

        game_t::players[local_id] = NULL;
        // TODO: Free const char *name?
        players.remove(local_id);

        FL_FREE(p);
    }
}

void game_t::clear_players() {
    for (uint32_t i = 0; i < players.data_count; ++i) {
        remove_player(i);
    }

    players.clear();

    for (uint32_t i = 0; i < PLAYER_MAX_COUNT; ++i) {
        client_to_local_id_map[i] = -1;
    }
}

int32_t game_t::client_to_local_id(uint16_t client_id) {
    return client_to_local_id_map[client_id];
}

player_t *game_t::get_player(int32_t local_id) {
    if (local_id >= 0) {
        return players[local_id];
    }
    else {
        return NULL;
    }
}

void game_t::spawn_rock(
    uint16_t client_id,
    const vector3_t &position,
    const vector3_t &start_direction,
    const vector3_t &up) {
    rock_t r = {};
    r.flags.active = 1;
    r.flags.spawned_locally = 1;
    r.position = position;
    r.direction = start_direction;
    r.up = up;
    r.client_id = client_id;

    uint32_t idx = local_rocks.add();
    local_rocks[idx] = r;

    newly_spawned_rocks[newly_spawned_rock_count++] = idx;
}

void game_t::clear_newly_spawned_rocks() {
    newly_spawned_rock_count = 0;
}

void game_t::clear_chunks() {
    if (chunks.data_count) {
        chunks.clear();
        chunk_indices.clear();
    }
}

chunk_t *game_t::get_chunk(
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
        chunk_init(chunk, i, coord);

        chunk_indices.insert(hash, i);

        return chunk;
    }
}

chunk_t *game_t::access_chunk(
    const ivector3_t &coord) {
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

chunk_t **game_t::get_active_chunks(
    uint32_t *count) {
    *count = chunks.data_count;
    return chunks.data;
}

chunk_t **game_t::get_modified_chunks(
    uint32_t *count) {
    *count = modified_chunk_count;
    return modified_chunks;
}
 
void game_t::reset_modification_tracker() {
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
