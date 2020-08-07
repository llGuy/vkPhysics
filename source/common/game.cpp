#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"

static float dt;
static uint64_t current_tick;

void game_memory_init() {
    current_tick = 0;
}

void game_begin(game_initiate_info_t *info) {
    // And do some various things depending on the game_info_t
    // Like loading a map or something
}

void game_end() {
    
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
