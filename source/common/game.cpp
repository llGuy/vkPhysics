#include "map.hpp"
#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"

static float dt;
static uint64_t current_tick;

static game_mode_t game_mode;
static uint32_t team_count;
static team_t *teams;
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
}

void game_configure_team(
    uint32_t team_id,
    team_color_t color,
    uint32_t player_count) {
    team_t *t = &teams[team_id];
    t->init(color, player_count);
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

void join_game() {
    
}

void leave_game() {
    
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
