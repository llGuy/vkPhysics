#pragma once

#include <stdint.h>
#include <common/math.hpp>

struct event_submissions_t;

namespace app {

/*
  Gets updated everyime app::poll_input_events() gets called with
  the time elapsed (in seconds) between frames.
 */
extern float g_delta_time;

/*
  Updates raw_input_t structure and g_delta_time.
 */
void poll_input_events(event_submissions_t *events);

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

/*
  Each button_type_t will have a respective button_input_t structure
  containing useful information like press time (down_amount), and other
  flags which may be of use.
 */
struct button_input_t {
    button_state_t state = BS_NOT_DOWN;
    float down_amount = 0.0f;
    uint8_t instant: 4;
    uint8_t release: 4;
};

constexpr uint32_t MAX_CHARS = 10;

/*
  Structure contains all "raw" input events (keys, mouse buttons).
 */
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

/*
  Sets the 'i'th char in raw_input_t::char_stack to 0
 */
void nullify_char_at(uint32_t i);

const raw_input_t *get_raw_input();

void enable_cursor();
void disable_cursor();

/* 
   Initialises stuff like:
   - default window dimensions
   - default input settings (which keys correspond to which actions, etc...) 
*/
void init_settings();

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

    // Whatever is the "shoot" action for a weapon (default: mouse left)
    GIAT_TRIGGER1,
    // Whatever the other ability of a weapon is (default: mouse right)
    GIAT_TRIGGER2,
    // Unused
    GIAT_TRIGGER3,
    // Jump
    GIAT_TRIGGER4,
    // Switch shapes
    GIAT_TRIGGER5,
    // Crouch
    GIAT_TRIGGER6,
    // Unused
    GIAT_TRIGGER7,

    // Switch weapons
    GIAT_TRIGGER8,
    GIAT_NUMBER0,
    GIAT_NUMBER1,
    GIAT_NUMBER2,
    // Invalid
    GIAT_INVALID_ACTION
};

struct game_input_action_t {
    button_state_t state;
    float down_amount = 0.0f;
    bool instant = 0;
    bool release = 0;
};

// To translate from raw_input_t
struct game_input_t {
    game_input_action_t actions[GIAT_INVALID_ACTION];
    float previous_mouse_x;
    float previous_mouse_y;
    float mouse_x;
    float mouse_y;
};

const game_input_t *get_game_input();

/*
  Translates the raw input contained in raw_input_t to game_input_t
 */
void translate_input();

}
