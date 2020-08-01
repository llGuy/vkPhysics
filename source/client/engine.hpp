#pragma once

#include <common/tools.hpp>

enum game_init_flags_t : int32_t {
    GIF_WINDOWED = 1 << 0,
    GIF_NOT_WINDOWED = 1 << 1,
    GIF_CLIENT = 1 << 2,
    GIF_SERVER = 1 << 3
};

struct game_init_data_t {
    uint32_t fl_pool_size;
    uint32_t ln_pool_size;
    int32_t flags;

    uint32_t argc;
    char **argv;

    // Optional data (could be for example android_app)
    void *main_data;
};

void game_main(
    game_init_data_t *game_init_data);

// Make sure that this gets thought through in entire engine
float logic_delta_time();
uint64_t &get_current_tick();
game_init_flags_t get_game_init_flags();

// Engine settings:
enum graphics_quality_t {
    GQ_FANCY,
    GQ_LOW
};

struct settings_t {
    // Graphics
    float max_frame_rate;
    graphics_quality_t graphics_quality;

    // Controls
    float mouse_sensitivity;

    // Startup
    bool start_fullscreen;

    // Misc
    bool show_debug_window;
};

enum highlevel_focus_t {
    HF_WORLD, HF_UI
};

struct game_flags_t {
    uint32_t running: 1;
    // Is it UI, or gameplay
    // If it's UI, enable mouse, if not, disable mouse
    uint32_t focus: 1;
    // Are we in the startup screen?
    // In which case, we need to render the startup world
    // If not, render the world that is currently loaded in
    uint32_t startup: 1;
};
