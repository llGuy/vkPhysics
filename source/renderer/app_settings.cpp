#include "app_context.hpp"

namespace app {

static game_input_t game_input;

// Settings
struct action_bound_mouse_keyboard_button_t {
    // Keyboard/Mouse button
    button_type_t bound_button;
};

static action_bound_mouse_keyboard_button_t bound_key_mouse_buttons[GIAT_INVALID_ACTION];

void init_settings() {
    bound_key_mouse_buttons[GIAT_OK].bound_button = BT_ENTER;
    bound_key_mouse_buttons[GIAT_CANCEL].bound_button = BT_ESCAPE;
    bound_key_mouse_buttons[GIAT_MENU].bound_button = BT_ESCAPE;

    bound_key_mouse_buttons[GIAT_MOVE_FORWARD].bound_button = BT_W;
    bound_key_mouse_buttons[GIAT_MOVE_LEFT].bound_button = BT_A;
    bound_key_mouse_buttons[GIAT_MOVE_BACK].bound_button = BT_S;
    bound_key_mouse_buttons[GIAT_MOVE_RIGHT].bound_button = BT_D;

    bound_key_mouse_buttons[GIAT_TRIGGER1].bound_button = BT_MOUSE_LEFT;
    bound_key_mouse_buttons[GIAT_TRIGGER2].bound_button = BT_MOUSE_RIGHT;
    bound_key_mouse_buttons[GIAT_TRIGGER3].bound_button = BT_R;
    bound_key_mouse_buttons[GIAT_TRIGGER4].bound_button = BT_SPACE;
    bound_key_mouse_buttons[GIAT_TRIGGER5].bound_button = BT_E;
    bound_key_mouse_buttons[GIAT_TRIGGER6].bound_button = BT_LEFT_SHIFT;
    bound_key_mouse_buttons[GIAT_TRIGGER7].bound_button = BT_G;

    bound_key_mouse_buttons[GIAT_TRIGGER8].bound_button = BT_Q;
    bound_key_mouse_buttons[GIAT_NUMBER0].bound_button = BT_ONE;
    bound_key_mouse_buttons[GIAT_NUMBER1].bound_button = BT_TWO;
    bound_key_mouse_buttons[GIAT_NUMBER2].bound_button = BT_THREE;
}

static void s_set_button_action_state(
    uint32_t action,
    const raw_input_t *raw_input) {
    auto *raw_key_mouse_input = &raw_input->buttons[bound_key_mouse_buttons[action].bound_button];

    game_input.actions[action].state = raw_key_mouse_input->state;
    game_input.actions[action].down_amount = raw_key_mouse_input->down_amount;
    game_input.actions[action].instant = raw_key_mouse_input->instant;
    game_input.actions[action].release = raw_key_mouse_input->release;
}

void translate_input() {
    auto *raw_input = get_raw_input();

    for (uint32_t action = 0; action < GIAT_INVALID_ACTION; ++action) {
        s_set_button_action_state(action, raw_input);
    }

    game_input.previous_mouse_x = raw_input->previous_cursor_pos_x;
    game_input.previous_mouse_y = raw_input->previous_cursor_pos_y;
    game_input.mouse_x = raw_input->cursor_pos_x;
    game_input.mouse_y = raw_input->cursor_pos_y;
}

const game_input_t *get_game_input() {
    return &game_input;
}

}
