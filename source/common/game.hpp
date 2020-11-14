#pragma once

#include "math.hpp"
#include "team.hpp"
#include "player.hpp"
#include "constant.hpp"
#include "containers.hpp"

// Need to call this at the start of application
void game_memory_init();

enum class game_mode_t { DEATHMATCH, CAPTURE_THE_FLAG, INVALID };

// First configure the game
void game_configure_game_mode(game_mode_t mode);
void game_configure_map(const char *map_path);
void game_configure_team_count(uint32_t team_count);
void game_configure_team(uint32_t team_index, team_color_t color, uint32_t player_count);

// Methods that the server will use
void game_start();
// Ends a game, no more players can join this game
void game_stop();
/////////////////////////////

void game_set_teams(
    uint32_t team_count,
    team_info_t *team_infos);

void game_change_player_team(player_t *player, team_color_t color);
void game_add_player_to_team(player_t *player, team_color_t color);
void game_remove_player_from_team(player_t *player);
bool game_check_team_joinable(team_color_t color);

void timestep_begin(float dt);
void timestep_end();
float get_game_timestep_delta();
uint64_t &get_current_tick();
team_t *get_teams(uint32_t *team_count);

extern uint64_t g_current_tick;
