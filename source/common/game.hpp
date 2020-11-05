#pragma once

#include "math.hpp"
#include "constant.hpp"
#include "containers.hpp"
#include "team.hpp"

// Need to call this at the start of application
void game_memory_init();

enum class game_mode_t { DEATHMATCH, CAPTURE_THE_FLAG, INVALID };

// First configure the game
void game_configure_game_mode(game_mode_t mode);
void game_configure_map(const char *map_path);
void game_configure_team_count(uint32_t team_count);
void game_configure_team(uint32_t team_index, team_color_t color, uint32_t player_count);

// Ends a game, no more players can join this game
void game_start();
void game_stop();

void join_game();
void leave_game();

void timestep_begin(float dt);
void timestep_end();
float get_game_timestep_delta();
uint64_t &get_current_tick();

extern uint64_t g_current_tick;
