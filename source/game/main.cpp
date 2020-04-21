#include "engine.hpp"

int32_t main(
    int32_t argc,
    char *argv[]) {
    game_init_data_t game_init_data = {};
    game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

#if !LINK_AGAINST_RENDERER
    game_init_data.flags = GIF_NOT_WINDOWED | GIF_SERVER;
#endif

    game_init_data.argc = argc;
    game_init_data.argv = argv;

    game_main(&game_init_data);
    
    return 0;
}
