#pragma once

#include <common/tools.hpp>

uint64_t &get_current_tick();

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
