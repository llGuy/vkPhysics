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
    ui_box_t inner_background;
    ui_text_t text;
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

    float button_size = 0.15f;

    for (uint32_t i = 0; i < B_INVALID_BUTTON; ++i) {
        game_menu_buttons[i].background_box.init(
            RT_LEFT_UP,
            3.0f,
            ui_vector2_t(0.0f, (float)i * (-button_size)),
            ui_vector2_t(1.0f, 1.0f),
            &menu_box,
            0x07070736);

        game_menu_buttons[i].inner_background.init(
            RT_RELATIVE_CENTER,
            4.0f,
            ui_vector2_t(0.0f, 0.0f),
            ui_vector2_t(0.9f, 0.9f),
            &game_menu_buttons[i].background_box,
            0x04040436);
    }

    game_menu_buttons[B_SPAWN].text.init(
        &game_menu_buttons[B_SPAWN].inner_background,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f,
        0.9f,
        12,
        2.0f);

    game_menu_buttons[B_SPAWN].text.draw_string("SPAWN", 0xFFFFFFFF);

    game_menu_buttons[B_SELECT_TEAM].text.init(
        &game_menu_buttons[B_SELECT_TEAM].inner_background,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f,
        0.9f,
        12,
        2.0f);

    game_menu_buttons[B_SELECT_TEAM].text.draw_string("SELECT TEAM", 0xFFFFFFFF);
}

void u_submit_game_menu() {
    push_colored_ui_box(
        &menu_box);

    mark_ui_textured_section(
        u_game_font()->font_img.descriptor);

    for (uint32_t i = 0; i < B_INVALID_BUTTON; ++i) {
        push_colored_ui_box(
            &game_menu_buttons[i].inner_background);

        push_ui_text(
            &game_menu_buttons[i].text);

    }
}

void u_game_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    
}
