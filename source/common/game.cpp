#include "common/containers.hpp"
#include "map.hpp"
#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include <math.h>
#include <bits/stdint-uintn.h>

static float dt;
static uint64_t current_tick;

static game_mode_t game_mode;
static uint32_t team_count;
static team_t *teams;
static uint32_t team_color_to_index[(uint32_t)team_color_t::COUNT];

static const char *current_map_path;
static map_t *current_map_data;

void game_memory_init() {
    current_tick = 0;
    load_map_names();
    game_mode = game_mode_t::INVALID;
}

void game_configure_game_mode(game_mode_t mode) {
    game_mode = mode;
}

void game_configure_map(const char *map_path) {
    current_map_path = map_path;
}

void game_configure_team_count(uint32_t count) {
    team_count = count;
    teams = FL_MALLOC(team_t, team_count);
    memset(teams, 0, sizeof(team_t) * team_count);
    memset(team_color_to_index, 0, sizeof(uint32_t) * (uint32_t)team_color_t::COUNT);
}

void game_configure_team(
    uint32_t team_id,
    team_color_t color,
    uint32_t player_count) {
    team_t *t = &teams[team_id];
    t->init(color, player_count, 0);
    team_color_to_index[color] = team_id;
}

void game_start() {
    if (current_map_path)
        current_map_data = load_map(current_map_path);

    current_tick = 0;
}

void game_stop() {
    // Deinitialise everything in the game, unload map, etc...
    unload_map(current_map_data);
    current_map_data = NULL;
    game_mode = game_mode_t::INVALID;
    team_count = 0;
    FL_FREE(teams);
}

void game_set_teams(
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

void game_add_player_to_team(player_t *player) {
    team_color_t color = (team_color_t)player->flags.team_color;
    if (color > team_color_t::INVALID && color < team_color_t::COUNT) {
        uint32_t index = team_color_to_index[color];
        teams[index].add_player(player->local_id);
    }
}

bool game_check_team_joinable(team_color_t color) {
    // Make sure that player count in team == min(player counts of teams)
    uint32_t min_player_count = teams[0].player_count();
    
    for (uint32_t i = 1; i < team_count; ++i) {
        min_player_count = MIN(min_player_count, teams[i].player_count());
    }

    uint32_t team_index = team_color_to_index[color];

    return (min_player_count == teams[team_index].player_count());
}

void timestep_begin(float dt_in) {
    dt = dt_in;
}

void timestep_end() {
    ++current_tick;
}

float get_game_timestep_delta() {
    return dt;
}

uint64_t &get_current_tick() {
    return current_tick;
}

team_t *get_teams(uint32_t *team_count_ptr) {
    *team_count_ptr = team_count;
    return teams;
}

