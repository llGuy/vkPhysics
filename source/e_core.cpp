// Engine core

#include "engine.hpp"
#include "e_internal.hpp"

// Gonna use these when multithreading gets added
// Logic delta time
static float ldelta_time;
// Surface delta time
static float sdelta_time;

void game_main(
    game_init_data_t *game_init_data) {
    e_event_system_init();
}

void run_game() {
    
}

void finish_game() {
    
}

float surface_delta_time() {
    return sdelta_time;
}

float logic_delta_time() {
    return ldelta_time;
}
