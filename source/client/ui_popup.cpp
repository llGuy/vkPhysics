#include "ui_menu_layout.hpp"
#include "ui_core.hpp"
#include "ui_popup.hpp"
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

#define MAX_POPUPS 10

static uint32_t popup_count;
static ui_popup_t popups[MAX_POPUPS] = {};

#define INVALID_TYPING_INDEX 0xFFFF

void ui_popups_init() {
    popup_count = 0;
    memset(popups, 0, sizeof(ui_popup_t) * MAX_POPUPS);
}

ui_popup_t *ui_add_popup(uint32_t vertical_section_count) {
    ui_popup_t *popup = &popups[popup_count++];
    memset(popup, 0, sizeof(ui_popup_t));

    popup->vertical_section_count = 0;
    popup->current_typing_section = INVALID_TYPING_INDEX;

    ui_push_panel(USI_POPUP);

    return popup;
}

void ui_push_popup_section_button_double(
    ui_popup_t *popup,
    const char **button_text,
    void (** handle_press_proc)(ui_popup_t *, event_submissions_t *)) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_BUTTON_DOUBLE;

    for (uint32_t i = 0; i < 2; ++i) {
        // Don't draw string just yet because ui_text_t may not have been initialised yet
        section->d_button.buttons[i].button_string = button_text[i];
        section->d_button.buttons[i].handle_press_proc = handle_press_proc[i];
    }
}

void ui_push_popup_section_button_single(
    ui_popup_t *popup,
    const char *button_text,
    void (* handle_press_proc)(ui_popup_t *, event_submissions_t *)) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_BUTTON_SINGLE;

    // Don't draw string just yet because ui_text_t may not have been initialised yet
    section->s_button.button.button_string = button_text;
    section->s_button.button.handle_press_proc = handle_press_proc;
}

void ui_push_popup_section_text(ui_popup_t *popup, const char *text) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_TEXT;
    section->text.string = text;
}

void ui_push_popup_section_input(ui_popup_t *popup) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_INPUT;
}

void ui_prepare_popup_for_render(ui_popup_t *popup) {
    { // Initialise popup main panel
        popup->panel.init(RT_CENTER, 1.0f, ui_vector2_t(0.0f, 0.0f), ui_vector2_t(0.6f, 0.6f), NULL, 0x000000EE);
    }

    // Initialise each section
    // Y difference between each section is 0.1f
    float current_y = 0.1 * (float)popup->vertical_section_count;
    float start_x = 0.0f;
    for (uint32_t i = 0; i < popup->vertical_section_count; ++i) {
        popup_section_t *section = &popup->sections[i];

        switch (section->type) {
        case PST_BUTTON_SINGLE: {
            auto *button = &section->s_button.button;
            button->box.init(
                RT_RELATIVE_CENTER,
                6.0f,
                ui_vector2_t(start_x, current_y),
                ui_vector2_t(0.9f, 0.2f),
                &popup->panel,
                0x09090956);

            button->text.init(
                &button->box,
                ui_game_font(),
                ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f, 25, 1.8f);

            if (!button->text.char_count) {
                button->text.draw_string(button->button_string, 0xFFFFFFFF);
                button->text.null_terminate();
            }

            button->color.init(0x05050536, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
        } break;

        case PST_BUTTON_DOUBLE: {
            { // First button
                start_x = -0.22f;
                auto *button = &section->d_button.buttons[0];
                button->box.init(
                    RT_RELATIVE_CENTER,
                    3.0f,
                    ui_vector2_t(start_x, current_y),
                    ui_vector2_t(0.4f, 0.2f),
                    &popup->panel,
                    0x09090956);

                button->text.init(
                    &button->box,
                    ui_game_font(),
                    ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                    0.8f, 0.9f, 7, 1.8f);

                if (!button->text.char_count) {
                    button->text.draw_string(button->button_string, 0xFFFFFFFF);
                    button->text.null_terminate();
                }

                button->color.init(0x05050536, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
            }
            
            { // Second button
                start_x = 0.22f;
                auto *button = &section->d_button.buttons[1];
                button->box.init(
                    RT_RELATIVE_CENTER,
                    3.0f,
                    ui_vector2_t(start_x, current_y),
                    ui_vector2_t(0.4f, 0.2f),
                    &popup->panel,
                    0x09090956);

                button->text.init(
                    &button->box,
                    ui_game_font(),
                    ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                    0.8f, 0.9f, 7, 1.8f);

                if (!button->text.char_count) {
                    button->text.draw_string(button->button_string, 0xFFFFFFFF);
                    button->text.null_terminate();
                }

                button->color.init(0x05050536, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
            }

            start_x = 0.0f;
        } break;

        case PST_TEXT: {
            auto *text = &section->text;
            
            text->box.init(
                RT_RELATIVE_CENTER, 8.0f,
                ui_vector2_t(start_x, current_y),
                ui_vector2_t(0.9f, 0.2f),
                &popup->panel, 0x09090936);

            text->text.init(
                &text->box,
                ui_game_font(),
                ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f, 25, 1.8f);

            if (!text->text.char_count) {
                text->text.draw_string(text->string, 0xFFFFFFFF);
                text->text.null_terminate();
            }
        } break;

        case PST_INPUT: {
            auto *input = &section->input;
            
            input->box.init(RT_RELATIVE_CENTER, 8.0f, ui_vector2_t(start_x, current_y), ui_vector2_t(0.9f, 0.2f), &popup->panel, 0x09090936);
            input->color.init(0x09090936, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
            input->input.text.init(
                &input->box, ui_game_font(),
                ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f, 25, 1.8f);

            input->input.text_color = 0xFFFFFFFF;
            input->input.cursor_position = 0;
        } break;
        }

        current_y -= 0.15f;
    }
}

void ui_submit_popups() {
    mark_ui_textured_section(ui_game_font()->font_img.descriptor);

    for (uint32_t i = 0; i < popup_count; ++i) {
        ui_popup_t *popup = &popups[i];

        push_colored_ui_box(&popup->panel);
        for (uint32_t i = 0; i < popup->vertical_section_count; ++i) {
            popup_section_t *section = &popup->sections[i];

            switch (section->type) {
            case PST_BUTTON_SINGLE: {
                push_colored_ui_box(&section->s_button.button.box);
                push_ui_text(&section->s_button.button.text);
            } break;

            case PST_BUTTON_DOUBLE: {
                push_colored_ui_box(&section->d_button.buttons[0].box);
                push_ui_text(&section->d_button.buttons[0].text);

                push_colored_ui_box(&section->d_button.buttons[1].box);
                push_ui_text(&section->d_button.buttons[1].text);
            } break;

            case PST_TEXT: {
                push_ui_text(&section->text.text);
            } break;

            case PST_INPUT: {
                push_ui_input_text(1, 0, 0xFFFFFFFF, &section->input.input);
            } break;
            }
        }
    }
}

void ui_popup_input(event_submissions_t *events, raw_input_t *input) {
    if (popup_count) {
        ui_popup_t *popup = &popups[popup_count - 1];

        bool hovering = 0;

        void (* input_proc)(ui_popup_t *, event_submissions_t *) = NULL;
        uint32_t hovering_over_typing = 0;

        for (uint32_t i = 0; i < popup->vertical_section_count; ++i) {
            popup_section_t *section = &popup->sections[i];

            popup->dummy = i;

            switch (section->type) {
            case PST_BUTTON_SINGLE: {
                if ((hovering = ui_hover_over_box(
                    &section->s_button.button.box,
                    input->cursor_pos_x,
                    input->cursor_pos_y))) {
                    input_proc = section->s_button.button.handle_press_proc;
                }

                color_pair_t color = section->s_button.button.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                section->s_button.button.box.color = color.current_background;
            } break;

            case PST_BUTTON_DOUBLE: {
                for (uint32_t j = 0; j < 2; ++j) {
                    if ((hovering = ui_hover_over_box(
                        &section->d_button.buttons[j].box,
                        input->cursor_pos_x,
                        input->cursor_pos_y))) {
                        input_proc = section->d_button.buttons[j].handle_press_proc;
                    }

                    color_pair_t color = section->d_button.buttons[j].color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                    section->d_button.buttons[j].box.color = color.current_background;
                }
            } break;

            case PST_INPUT: {
                if ((hovering = ui_hover_over_box(
                    &section->input.box,
                    input->cursor_pos_x,
                    input->cursor_pos_y))) {
                    input_proc = [](ui_popup_t *popup, event_submissions_t *) {
                        popup->current_typing_section = popup->dummy;
                    };

                    hovering_over_typing = 1;
                }

                color_pair_t color = section->input.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                section->input.box.color = color.current_background;
            } break;
            }
        }

        if (input->buttons[BT_MOUSE_LEFT].instant)
            if (input_proc)
                input_proc(popup, events);

        if (!hovering_over_typing) {
            popup->current_typing_section = INVALID_TYPING_INDEX;
        }

        if (popup->current_typing_section != INVALID_TYPING_INDEX) {
            popup->sections[popup->current_typing_section].input.input.input(input);
            //---------------------------------------------^^^^^^^^^^^^^^^^^ yeesh lol
        }
    }
}
