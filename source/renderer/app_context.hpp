#pragma once

#include <stdint.h>
#include <common/math.hpp>

#include "vk_context.hpp"

struct event_submissions_t;

namespace app {

void api_init();

extern float g_delta_time;

struct context_info_t {
    void *window;
    const char *name;
    uint32_t width, height;
    vk::surface_proc_t surface_proc;
};

context_info_t init_context();

enum button_type_t {
    BT_A, BT_B, BT_C, BT_D, BT_E, BT_F, BT_G, BT_H, BT_I, BT_J,
    BT_K, BT_L, BT_M, BT_N, BT_O, BT_P, BT_Q, BT_R, BT_S, BT_T,
    BT_U, BT_V, BT_W, BT_X, BT_Y, BT_Z,

    BT_ZERO, BT_ONE, BT_TWO, BT_THREE, BT_FOUR, BT_FIVE, BT_SIX,
    BT_SEVEN, BT_EIGHT, BT_NINE, BT_UP, BT_LEFT, BT_DOWN, BT_RIGHT,
    BT_SPACE, BT_LEFT_SHIFT, BT_LEFT_CONTROL, BT_ENTER, BT_BACKSPACE,
    BT_ESCAPE, BT_F1, BT_F2, BT_F3, BT_F4, BT_F5, BT_F9, BT_F11,

    BT_MOUSE_LEFT, BT_MOUSE_RIGHT, BT_MOUSE_MIDDLE, BT_MOUSE_MOVE_UP,
    BT_MOUSE_MOVE_LEFT, BT_MOUSE_MOVE_DOWN, BT_MOUSE_MOVE_RIGHT, BT_INVALID_KEY
};

enum button_state_t : char { BS_NOT_DOWN, BS_DOWN };

struct button_input_t {
    button_state_t state = BS_NOT_DOWN;
    float down_amount = 0.0f;
    uint8_t instant: 4;
    uint8_t release: 4;
};

constexpr uint32_t MAX_CHARS = 10;

// Input that is stored in this struct hasn't been translated into game input
struct raw_input_t {
    button_input_t buttons[BT_INVALID_KEY] = {};
    uint32_t instant_count = 0;
    uint32_t instant_indices[BT_INVALID_KEY] = {};
    uint32_t release_count = 0;
    uint32_t release_indices[BT_INVALID_KEY] = {};

    uint32_t char_count;
    char char_stack[10] = {};

    bool cursor_moved = 0;
    float cursor_pos_x = 0.0f, cursor_pos_y = 0.0f;
    float previous_cursor_pos_x = 0.0f, previous_cursor_pos_y = 0.0f;

    bool in_fullscreen = 0;
    int32_t desktop_window_width, desktop_window_height;
    int32_t window_pos_x, window_pos_y;
    
    bool resized = 0;
    int32_t window_width, window_height;

    vector2_t normalized_cursor_position;

    float dt;

    bool show_cursor = 1;

    bool toggled_fullscreen = 0;
    bool has_focus = 1;
};

const raw_input_t *get_raw_input();

// Updates raw_input data
void poll_input_events(event_submissions_t *events);

void enable_cursor();
void disable_cursor();

void get_immediate_cursor_pos(double *x, double *y);

}
