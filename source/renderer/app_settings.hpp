#pragma once

#include "app_context.hpp"

namespace app {

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

void translate_input();

}
