#include "ux.hpp"
#include "ux_popup.hpp"
#include "ui_submit.hpp"

namespace ux {

static constexpr uint32_t MAX_POPUPS = 10;
static constexpr uint32_t INVALID_TYPING_INDEX = 0xFFFF;

static uint32_t popup_count;
static popup_t popups[MAX_POPUPS] = {};

void init_popups() {
    popup_count = 0;
    memset(popups, 0, sizeof(popup_t) * MAX_POPUPS);
}

popup_t *add_popup(uint32_t vertical_section_count) {
    popup_t *popup = &popups[popup_count++];
    memset(popup, 0, sizeof(popup_t));

    popup->vertical_section_count = 0;
    popup->current_typing_section = INVALID_TYPING_INDEX;

    push_panel(SI_POPUP);

    return popup;
}

void push_popup_section_button_double(
    popup_t *popup,
    const char **button_text,
    void (** handle_press_proc)(popup_t *)) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_BUTTON_DOUBLE;

    for (uint32_t i = 0; i < 2; ++i) {
        // Don't draw string just yet because text_t may not have been initialised yet
        section->d_button.buttons[i].button_string = button_text[i];
        section->d_button.buttons[i].handle_press_proc = handle_press_proc[i];
    }
}

void push_popup_section_button_single(
    popup_t *popup,
    const char *button_text,
    void (* handle_press_proc)(popup_t *)) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_BUTTON_SINGLE;

    // Don't draw string just yet because text_t may not have been initialised yet
    section->s_button.button.button_string = button_text;
    section->s_button.button.handle_press_proc = handle_press_proc;
}

void push_popup_section_text(popup_t *popup, const char *text) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_TEXT;
    section->text.string = text;
}

void push_popup_section_input(popup_t *popup) {
    popup_section_t *section = &popup->sections[popup->vertical_section_count++];
    memset(section, 0, sizeof(popup_section_t));

    section->type = PST_INPUT;
}

void prepare_popup_for_render(popup_t *popup) {
    { // Initialise popup main panel
        popup->panel.init(ui::RT_CENTER, 1.0f, ui::vector2_t(0.0f, 0.0f), ui::vector2_t(0.6f, 0.6f), NULL, 0x000000EE);
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
                ui::RT_RELATIVE_CENTER,
                6.0f,
                ui::vector2_t(start_x, current_y),
                ui::vector2_t(0.9f, 0.2f),
                &popup->panel,
                0x09090956);

            button->text.init(
                &button->box,
                get_game_font(),
                ui::text_t::font_stream_box_relative_to_t::BOTTOM,
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
                    ui::RT_RELATIVE_CENTER,
                    3.0f,
                    ui::vector2_t(start_x, current_y),
                    ui::vector2_t(0.4f, 0.2f),
                    &popup->panel,
                    0x09090956);

                button->text.init(
                    &button->box,
                    get_game_font(),
                    ui::text_t::font_stream_box_relative_to_t::BOTTOM,
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
                    ui::RT_RELATIVE_CENTER,
                    3.0f,
                    ui::vector2_t(start_x, current_y),
                    ui::vector2_t(0.4f, 0.2f),
                    &popup->panel,
                    0x09090956);

                button->text.init(
                    &button->box,
                    get_game_font(),
                    ui::text_t::font_stream_box_relative_to_t::BOTTOM,
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
                ui::RT_RELATIVE_CENTER, 8.0f,
                ui::vector2_t(start_x, current_y),
                ui::vector2_t(0.9f, 0.2f),
                &popup->panel, 0x09090936);

            text->text.init(
                &text->box,
                get_game_font(),
                ui::text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f, 25, 1.8f);

            if (!text->text.char_count) {
                text->text.draw_string(text->string, 0xFFFFFFFF);
                text->text.null_terminate();
            }
        } break;

        case PST_INPUT: {
            auto *input = &section->input;
            
            input->box.init(ui::RT_RELATIVE_CENTER, 8.0f, ui::vector2_t(start_x, current_y), ui::vector2_t(0.9f, 0.2f), &popup->panel, 0x09090936);
            input->color.init(0x09090936, MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR, 0xFFFFFFFF, 0xFFFFFFFF);
            input->input.text.init(
                &input->box, get_game_font(),
                ui::text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f, 25, 1.8f);

            input->input.text_color = 0xFFFFFFFF;
            input->input.cursor_position = 0;
        } break;
        }

        current_y -= 0.15f;
    }
}

void submit_popups() {
    ui::mark_ui_textured_section(get_game_font()->font_img.descriptor);

    for (uint32_t i = 0; i < popup_count; ++i) {
        popup_t *popup = &popups[i];

        ui::push_color_box(&popup->panel);
        for (uint32_t i = 0; i < popup->vertical_section_count; ++i) {
            popup_section_t *section = &popup->sections[i];

            switch (section->type) {
            case PST_BUTTON_SINGLE: {
                ui::push_color_box(&section->s_button.button.box);
                ui::push_text(&section->s_button.button.text);
            } break;

            case PST_BUTTON_DOUBLE: {
                ui::push_color_box(&section->d_button.buttons[0].box);
                ui::push_text(&section->d_button.buttons[0].text);

                ui::push_color_box(&section->d_button.buttons[1].box);
                ui::push_text(&section->d_button.buttons[1].text);
            } break;

            case PST_TEXT: {
                ui::push_text(&section->text.text);
            } break;

            case PST_INPUT: {
                ui::push_ui_input_text(1, 0, 0xFFFFFFFF, &section->input.input);
            } break;
            }
        }
    }
}

void popup_input(const app::raw_input_t *input) {
    if (popup_count) {
        popup_t *popup = &popups[popup_count - 1];

        bool hovering = 0;

        void (* input_proc)(popup_t *) = NULL;
        uint32_t hovering_over_typing = 0;

        for (uint32_t i = 0; i < popup->vertical_section_count; ++i) {
            popup_section_t *section = &popup->sections[i];

            popup->dummy = i;

            switch (section->type) {
            case PST_BUTTON_SINGLE: {
                if ((hovering = is_hovering_over_box(
                    &section->s_button.button.box,
                    vector2_t(input->cursor_pos_x, input->cursor_pos_y)))) {
                    input_proc = section->s_button.button.handle_press_proc;
                }

                color_pair_t color = section->s_button.button.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                section->s_button.button.box.color = color.current_background;
            } break;

            case PST_BUTTON_DOUBLE: {
                for (uint32_t j = 0; j < 2; ++j) {
                    if ((hovering = is_hovering_over_box(
                        &section->d_button.buttons[j].box,
                        vector2_t(input->cursor_pos_x, input->cursor_pos_y)))) {
                        input_proc = section->d_button.buttons[j].handle_press_proc;
                    }

                    color_pair_t color = section->d_button.buttons[j].color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                    section->d_button.buttons[j].box.color = color.current_background;
                }
            } break;

            case PST_INPUT: {
                if ((hovering = is_hovering_over_box(
                    &section->input.box,
                    vector2_t(input->cursor_pos_x, input->cursor_pos_y)))) {
                    input_proc = [](popup_t *popup) {
                        popup->current_typing_section = popup->dummy;
                    };

                    hovering_over_typing = 1;
                }

                color_pair_t color = section->input.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovering);

                section->input.box.color = color.current_background;
            } break;
            }
        }

        if (input->buttons[app::BT_MOUSE_LEFT].instant)
            if (input_proc)
                input_proc(popup);

        if (!hovering_over_typing) {
            popup->current_typing_section = INVALID_TYPING_INDEX;
        }

        if (popup->current_typing_section != INVALID_TYPING_INDEX) {
            popup->sections[popup->current_typing_section].input.input.input(input);
            //---------------------------------------------^^^^^^^^^^^^^^^^^ yeesh
        }
    }
}

}
