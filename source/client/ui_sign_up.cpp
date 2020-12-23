#include "common/allocators.hpp"
#include "common/event.hpp"
#include "common/string.hpp"
#include "ui_core.hpp"
#include "ui_menu_layout.hpp"
#include <cstdio>
#include <sha1.hpp>
#include <common/meta.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

enum button_t { B_SIGNUP, B_LOGIN, B_USERNAME, B_PASSWORD, B_INVALID };

static ui_box_t panel_box;

static void s_panel_init() {
    panel_box.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.6f, 0.6f),
        NULL,
        0x000000EE);
}

static ui_box_t signup_box;
static ui_text_t signup_text;
static widget_color_t signup_color;

static ui_box_t login_box;
static ui_text_t login_text;
static widget_color_t login_color;

static void s_buttons_init() {
    signup_box.init(RT_LEFT_DOWN, 3.0f, ui_vector2_t(0.07f, 0.2f), ui_vector2_t(0.4f, 0.4f), &panel_box, 0x09090956);
    signup_color.init(0x05050536, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
    signup_text.init(&signup_box, ui_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        1.5f, 1.3f, 7, 1.8f);

    if (!signup_text.char_count) {
        signup_text.draw_string("SIGN UP", 0xFFFFFFFF);
        signup_text.null_terminate();
    }

    login_box.init(RT_RIGHT_DOWN, 3.0f, ui_vector2_t(-0.07f, 0.2f), ui_vector2_t(0.4f, 0.4f), &panel_box, 0x09090956);
    login_color.init(0x05050536, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
    login_text.init(&login_box, ui_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        1.5f, 1.3f, 7, 1.8f);

    if (!login_text.char_count) {
        login_text.draw_string("LOGIN", 0xFFFFFFFF);
        login_text.null_terminate();
    }
}

enum typing_box_t { TB_USERNAME, TB_PASSWORD, TB_INVALID };

// Typing sections
static ui_box_t type_username_box;
static ui_input_text_t type_username_text;
static widget_color_t type_username_color;
static ui_box_t prompt_username_box;
static ui_text_t prompt_username_text;

static ui_box_t type_password_box;
static ui_input_text_t type_password_text;
static widget_color_t type_password_color;
static ui_box_t prompt_password_box;
static ui_text_t prompt_password_text;

static typing_box_t currently_typing;

static void s_typing_boxes_init() {
    { // For typing username
        prompt_username_box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(0.0f, 0.4f), ui_vector2_t(0.9f, 0.2f), &panel_box, 0x09090936);
        prompt_username_text.init(
            &prompt_username_box, ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f, 0.9f, 25, 1.8f);

        if (!prompt_username_text.char_count) {
            prompt_username_text.draw_string("USERNAME:", 0xFFFFFFFF);
            prompt_username_text.null_terminate();
        }

        type_username_box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(0.0f, 0.3f), ui_vector2_t(0.9f, 0.2f), &panel_box, 0x09090936);
        type_username_color.init(0x09090936, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
        type_username_text.text.init(
            &type_username_box, ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f, 0.9f, 25, 1.8f);

        type_username_text.text_color = 0xFFFFFFFF;
        type_username_text.cursor_position = 0;
    }
 
    { // For typing password
        prompt_password_box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(0.0f, 0.2f), ui_vector2_t(0.9f, 0.2f), &panel_box, 0x09090936);
        prompt_password_text.init(
            &prompt_password_box, ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f, 0.9f, 25, 1.8f);

        if (!prompt_password_text.char_count) {
            prompt_password_text.draw_string("PASSWORD:", 0xFFFFFFFF);
            prompt_password_text.null_terminate();
        }

        type_password_box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(0.0f, 0.1f), ui_vector2_t(0.9f, 0.2f), &panel_box, 0x09090936);
        type_password_color.init(0x09090936, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
        type_password_text.text.init(
            &type_password_box, ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f, 0.9f, 25, 1.8f);

        type_password_text.text_color = 0xFFFFFFFF;
        type_password_text.cursor_position = 0;
    }

    currently_typing = TB_INVALID;
}

static ui_box_t error_box;
static ui_text_t error_text;
static request_error_t error_type;
static bool error_happened;

static void s_error_section_init() {
    error_box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(0.07f, 0.01f), ui_vector2_t(0.8f, 0.8f), &panel_box, 0x0);

    error_text.init(&error_box, ui_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        1.5f, 1.3f, 30, 1.8f);

    if (!error_text.char_count) {
        error_text.draw_string("Username already exists", 0xFF0000FF);
        error_text.null_terminate();
    }
}

void ui_sign_up_menu_init() {
    s_panel_init();
    s_buttons_init();
    s_typing_boxes_init();
    s_error_section_init();
}

void ui_submit_sign_up_menu() {
    push_color_ui_box(&panel_box);
    push_color_ui_box(&signup_box);
    push_color_ui_box(&login_box);
    push_color_ui_box(&type_password_box);
    push_color_ui_box(&type_username_box);
    
    mark_ui_textured_section(ui_game_font()->font_img.descriptor);

    push_ui_text(&signup_text);
    push_ui_text(&login_text);
    push_ui_text(&prompt_username_text);
    push_ui_text(&prompt_password_text);
    push_ui_input_text(1, 0, 0xFFFFFFFF, &type_username_text);
    push_ui_input_text(1, 1, 0xFFFFFFFF, &type_password_text);

    if (error_happened)
        push_ui_text(&error_text);
}

void ui_sign_up_menu_input(event_submissions_t *events, raw_input_t *input) {
    button_t hovering_over = B_INVALID;
    bool hovering = 0;

    vector2_t cursor_position = vector2_t(input->cursor_pos_x, input->cursor_pos_y);

    { // Hover over sign up button
        if ((hovering = ui_hover_over_box(
                 &signup_box,
                 cursor_position)))
            hovering_over = B_SIGNUP;

        color_pair_t color = signup_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

        signup_box.color = color.current_background;
    }

    { // Hover over sign up button
        if ((hovering = ui_hover_over_box(
                 &login_box,
                 cursor_position)))
            hovering_over = B_LOGIN;

        color_pair_t color = login_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

        login_box.color = color.current_background;
    }

    { // Hover over username box
        if ((hovering = ui_hover_over_box(
                 &type_username_box,
                 cursor_position)))
            hovering_over = B_USERNAME;

        color_pair_t color = type_username_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

        type_username_box.color = color.current_background;
    }

    { // Hover over password box
        if ((hovering = ui_hover_over_box(
                 &type_password_box,
                 cursor_position)))
            hovering_over = B_PASSWORD;

        color_pair_t color = type_password_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

        type_password_box.color = color.current_background;
    }

    if (input->buttons[BT_MOUSE_LEFT].instant) {
        switch (hovering_over) {
        case B_SIGNUP: case B_LOGIN: {
            currently_typing = TB_INVALID;
            // Request meta server if username is available, if not, re-prompt
            // Immediately SHA1 the password

            char *username = create_fl_string(type_username_text.text.characters);
            uint8_t *password_bin = LN_MALLOC(uint8_t, 20);
            memset(password_bin, 0, sizeof(uint8_t) * 20);

            SHA1_CTX ctx;
            SHA1Init(&ctx);

            SHA1Update(&ctx, (uint8_t *)type_password_text.text.characters, type_password_text.text.char_count);
            SHA1Final(password_bin, &ctx);

            char *password = FL_MALLOC(char, 41);
            memset(password, 0, sizeof(char) * 41);

            for (uint32_t i = 0; i < 20; ++i) {
                sprintf(password + (2 * i), "%02x", password_bin[i]);
            }

            if (hovering_over == B_SIGNUP) {
                event_attempt_sign_up_t *event_data = FL_MALLOC(event_attempt_sign_up_t, 1);
                event_data->username = username;
                event_data->password = password;

                submit_event(ET_ATTEMPT_SIGN_UP, event_data, events);
            }
            else {
                event_attempt_login_t *event_data = FL_MALLOC(event_attempt_login_t, 1);
                event_data->username = username;
                event_data->password = password;

                submit_event(ET_ATTEMPT_LOGIN, event_data, events);
            }

            error_happened = 0;
        } break;

        case B_USERNAME: {
            currently_typing = TB_USERNAME;
        } break;

        case B_PASSWORD: {
            currently_typing = TB_PASSWORD;
        } break;

        default: {
            currently_typing = TB_INVALID;
        } break;
        }
    }

    if (currently_typing == TB_USERNAME)
        type_username_text.input(input);
    else if (currently_typing == TB_PASSWORD)
        type_password_text.input(input);
}

void ui_handle_sign_up_failed(request_error_t ierror_type) {
    error_happened = 1;
    error_type = ierror_type;

    switch (error_type) {
    case RE_USERNAME_EXISTS: {
        error_text.char_count = 0;
        error_text.draw_string("Username already exists", 0xFF0000FF);
    } break;

    case RE_INCORRECT_PASSWORD_OR_USERNAME: {
        error_text.char_count = 0;
        error_text.draw_string("Incorrect username or password", 0xFF0000FF);
    } break;
    }
}
