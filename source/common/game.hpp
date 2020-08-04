#pragma once

#include "math.hpp"
#include "constant.hpp"
#include "containers.hpp"

// Need to call this at the start of application
void game_memory_init();

struct game_initiate_info_t {};

// If game_info_t is NULL, 
void game_initiate(game_initiate_info_t *info);
// Ends a game, no more players can join this game
void game_end();

void join_game();
void leave_game();

void timestep_begin(float dt);
void timestep_end();
float get_timestep_delta();
uint64_t &get_current_tick();
