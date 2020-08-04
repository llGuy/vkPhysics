#include <glm/glm.hpp>
#include <glm/detail/type_vec.hpp>

#ifdef __ANDROID__
// Android mobile

extern "C" {
    void android_main(
        struct android_app *app) {
        game_init_data_t game_init_data = {};
        // Can only be this
        game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

        // Need to pass android_app pointer
        game_init_data.main_data = (void *)app;

        game_main(&game_init_data);
    }
}

#else
// Laptop / Desktop

#include <glm/glm.hpp>

// int32_t main(
//     int32_t argc,
//     char *argv[]) {
//     game_init_data_t game_init_data = {};
//     game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

// #if !LINK_AGAINST_RENDERER
//     game_init_data.flags = GIF_NOT_WINDOWED | GIF_SERVER;
// #endif

//     game_init_data.argc = argc;
//     game_init_data.argv = argv;

//     game_main(&game_init_data);

//     return 0;
// }

#endif
