#include "game.hpp"
#include "event.hpp"
#include "chunk.hpp"
#include "player.hpp"

static float dt;

void game_memory_init() {
    chunk_memory_init();
    player_memory_init();
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

void timestep_begin(float dt) {
    dt = dt;
}

void timestep_end() {
    dt = 0.0f;
}

float get_timestep_delta() {
    return dt;
}
