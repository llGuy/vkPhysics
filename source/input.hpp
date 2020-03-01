#pragma once

#include <stdint.h>
#include "tools.hpp"

enum button_type_t { A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
                     ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, UP, LEFT, DOWN, RIGHT,
                     SPACE, LEFT_SHIFT, LEFT_CONTROL, ENTER, BACKSPACE, ESCAPE, F1, F2, F3, F4, F5,
                     MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_MOVE_UP, MOUSE_MOVE_LEFT, MOUSE_MOVE_DOWN, MOUSE_MOVE_RIGHT, INVALID_KEY };

enum button_state_t : char { NOT_DOWN, INSTANT, REPEAT, RELEASE };

struct button_input_t {
    button_state_t state = button_state_t::NOT_DOWN;
    float down_amount = 0.0f;
    float value = 0.0f;
};

enum gamepad_button_type_t { DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT,
                             START, BACK,
                             LEFT_THUMB, LEFT_SHOULDER,
                             RIGHT_THUMB, RIGHT_SHOULDER,
                             CONTROLLER_A, CONTROLLER_B, CONTROLLER_X, CONTROLLER_Y,
                             LTHUMB_MOVE_RIGHT, LTHUMB_MOVE_LEFT, LTHUMB_MOVE_UP, LTHUMB_MOVE_DOWN,
                             RTHUMB_MOVE_RIGHT, RTHUMB_MOVE_LEFT, RTHUMB_MOVE_UP, RTHUMB_MOVE_DOWN,
                             LEFT_TRIGGER, RIGHT_TRIGGER,
                             INVALID_BUTTON };

struct gamepad_button_input_t {
    button_state_t state = button_state_t::NOT_DOWN;
    float down_amount;
    float value = 0.0f;
};

#define MAX_CHARS 10

struct raw_input_t {
    button_input_t buttons[button_type_t::INVALID_KEY];

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


    uint32_t gamepad_packet_number;
    gamepad_button_input_t gamepad_buttons[gamepad_button_type_t::INVALID_BUTTON];
};
