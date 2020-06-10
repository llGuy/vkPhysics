#include "u_internal.hpp"
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

enum button_t {
    B_SPAWN,
    B_SELECT_TEAM,
    // ...
    B_INVALID_BUTTON
};

static ui_box_t main_box;
static ui_box_t menu_box;

static struct game_menu_button_t {
    ui_box_t background_box;
} game_menu_buttons[B_INVALID_BUTTON];

void u_game_menu_init() {
    main_box.init(
        RT_CENTER,
        2.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        NULL,
        0x09090936);

    menu_box.init(
        RT_RIGHT_UP,
        0.6f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(1.0f, 1.0f),
        &main_box,
        0x09090936);
}

void u_submit_game_menu() {
    push_colored_ui_box(
        &menu_box);
}

void u_game_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    
}
