#include <iostream>

#include "engine.hpp"

#include <common/allocators.hpp>

int32_t main(
    int32_t argc,
    char *argv[]) {
    game_init_data_t game_init_data = {};
    game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

    game_main(&game_init_data);
    
    return 0;
}
