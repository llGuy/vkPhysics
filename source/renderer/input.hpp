#pragma once

#include <stdint.h>
#include <common/tools.hpp>
#include <common/event.hpp>

#include "renderer.hpp"

struct input_interface_data_t {
    surface_proc_t surface_creation_proc;
    uint32_t surface_width;
    uint32_t surface_height;
    const char *application_name;
    void *window;
};

DECLARE_RENDERER_PROC(input_interface_data_t, input_interface_init, void);

DECLARE_VOID_RENDERER_PROC(void, game_input_settings_init, void);

DECLARE_VOID_RENDERER_PROC(void, poll_input_events,
    event_submissions_t *events);

DECLARE_VOID_RENDERER_PROC(void, disable_cursor_display, void);

DECLARE_VOID_RENDERER_PROC(void, enable_cursor_display, void);

DECLARE_RENDERER_PROC(float, get_current_time, void);

DECLARE_RENDERER_PROC(float, surface_delta_time, void);

// TODO: Will remove this in the future once proper engine input handling is implemented
DECLARE_RENDERER_PROC(void *, surface_window, void);

enum button_type_t {
    BT_A, BT_B, BT_C, BT_D, BT_E, BT_F, BT_G, BT_H, BT_I, BT_J, BT_K, BT_L, BT_M, BT_N, BT_O, BT_P, BT_Q, BT_R, BT_S, BT_T, BT_U, BT_V, BT_W, BT_X, BT_Y, BT_Z,
    BT_ZERO, BT_ONE, BT_TWO, BT_THREE, BT_FOUR, BT_FIVE, BT_SIX, BT_SEVEN, BT_EIGHT, BT_NINE, BT_UP, BT_LEFT, BT_DOWN, BT_RIGHT,
    BT_SPACE, BT_LEFT_SHIFT, BT_LEFT_CONTROL, BT_ENTER, BT_BACKSPACE, BT_ESCAPE, BT_F1, BT_F2, BT_F3, BT_F4, BT_F5, BT_F11,
    BT_MOUSE_LEFT, BT_MOUSE_RIGHT, BT_MOUSE_MIDDLE, BT_MOUSE_MOVE_UP, BT_MOUSE_MOVE_LEFT, BT_MOUSE_MOVE_DOWN, BT_MOUSE_MOVE_RIGHT, BT_INVALID_KEY };

enum button_state_t : char { BS_NOT_DOWN, BS_DOWN };

struct button_input_t {
    button_state_t state = BS_NOT_DOWN;
    float down_amount = 0.0f;
    bool instant;
};

#define MAX_CHARS 10

struct raw_input_t {
    button_input_t buttons[BT_INVALID_KEY] = {};
    uint32_t instant_count = 0;
    uint32_t instant_indices[BT_INVALID_KEY] = {};

    uint32_t char_count;
    char char_stack[10] = {};

    bool cursor_moved = 0;
    float cursor_pos_x = 0.0f, cursor_pos_y = 0.0f;
    float previous_cursor_pos_x = 0.0f, previous_cursor_pos_y = 0.0f;

    bool resized = 0;
    int32_t window_width, window_height;

    vector2_t normalized_cursor_position;

    float dt;

    bool show_cursor = 1;

    bool toggled_fullscreen = 0;
    bool in_fullscreen = 0;
    bool has_focus = 1;
};

enum game_input_action_type_t {
    // Menu stuff
    GIAT_OK,
    GIAT_CANCEL,
    GIAT_MENU,
    // Player stuff
    GIAT_MOVE_FORWARD,
    GIAT_MOVE_LEFT,
    GIAT_MOVE_BACK,
    GIAT_MOVE_RIGHT,
    GIAT_TRIGGER1,
    GIAT_TRIGGER2,
    GIAT_TRIGGER3,
    GIAT_TRIGGER4,
    GIAT_TRIGGER5,
    GIAT_TRIGGER6,
    // Invalid
    GIAT_INVALID_ACTION };

struct game_input_action_t {
    button_state_t state;
    float down_amount = 0.0f;
    bool instant = 0;
};

// To translate from raw_input_t to player_input_t
struct game_input_t {
    game_input_action_t actions[GIAT_INVALID_ACTION];
    float previous_mouse_x;
    float previous_mouse_y;
    float mouse_x;
    float mouse_y;
};

DECLARE_VOID_RENDERER_PROC(void, translate_raw_to_game_input, void);

DECLARE_RENDERER_PROC(raw_input_t *, get_raw_input, void);
DECLARE_RENDERER_PROC(game_input_t *, get_game_input, void);
