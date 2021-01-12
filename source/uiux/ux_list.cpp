#include "ux.hpp"
#include "ux_list.hpp"
#include "ui_submit.hpp"
#include "ux_menu_layout.hpp"

#include <allocators.hpp>

namespace ux {

void list_t::init(
    ui::box_t *parent,
    uint32_t right_buttons_count,
    const char **right_button_text,
    void (** handle_inputs)(list_t *),
    void (* fill_item)(list_item_t *item)) {
    { // Initialise the list box
        list_box.init(
            ui::RT_RIGHT_DOWN,
            2.1f,
            ui::vector2_t(-0.02f, 0.15f),
            ui::vector2_t(0.95f, 0.95f), parent, 0x09090936);
    }

    { // Initialise the typing box
        typing_box.box.init(
            ui::RT_LEFT_DOWN,
            6.0f,
            ui::vector2_t(0.02f, 0.03f),
            ui::vector2_t(0.28f, 0.2f),
            parent,
            0x09090936);

        typing_box.input_text.text.init(
            &typing_box.box,
            get_game_font(),
            ui::text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.9f,
            18,
            1.8f);

        typing_box.color.init(
            0x09090936,
            MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
            0xFFFFFFFF,
            0xFFFFFFFF);

        typing_box.input_text.text_color = 0xFFFFFFFF;
        typing_box.input_text.cursor_position = 0;
        typing_box.is_typing = 0;
    }

    { // Set default values for the list items
        item_count = 0;
        items = NULL;
        fill_item_proc = fill_item;
    }

    { // Initialise the buttons on the right of the list
        float dx = 0.23f;
        right_buttons = flmalloc<list_button_t>(right_buttons_count);
        button_count = right_buttons_count;

        for (uint32_t i = 0; i < right_buttons_count; ++i) {
            list_button_t *button = &right_buttons[i];

            button->handle_input_proc = handle_inputs[i];

            button->box.init(
                ui::RT_RIGHT_DOWN,
                4.0f,
                ui::vector2_t(-0.02f - ((float)i * dx), 0.03f),
                ui::vector2_t(0.2f, 0.2f),
                parent,
                0x09090936);

            button->text.init(
                &button->box,
                get_game_font(),
                ui::text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f, 0.9f,
                10, 1.8f);

            if (!button->text.char_count)
                button->text.draw_string(right_button_text[i], 0xFFFFFFFF);

            button->color.init(
                0x09090936,
                MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
                0xFFFFFFFF,
                0xFFFFFFFF);

            button->text.null_terminate();
        }
    }
}

void list_t::clear() {
    if (items) {
        item_count = 0;
        flfree(items);
    }
}

void list_t::begin(uint32_t count) {
    item_count = 0;
    items = flmalloc<list_item_t>(count);
}

void list_t::add(const void *data) {
    items[item_count].data = data;
    item_count++;
}

void list_t::end() {
    float server_button_height = 0.1f;

    for (uint32_t i = 0; i < item_count; ++i) {
        list_item_t *item = &items[i];

        item->box.init(
            ui::RT_LEFT_UP,
            21.3f,
            ui::vector2_t(0.0f, -server_button_height * (float)i),
            ui::vector2_t(1.0f, server_button_height),
            &list_box,
            0x05050536);

        item->text.init(
            &item->box,
            get_game_font(),
            ui::text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.8f,
            60,
            1.8f);

        fill_item_proc(item);

        item->button_color.init(
            0x05050536,
            MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
            0xFFFFFFFF,
            0xFFFFFFFF);
    }
}

#if 0
void list_tsubmit_typing_box(typing_box_t *box) {
    ui::push_color_box(&box->box);
    ui::mark_ui_textured_section(ui_game_font()->font_img.descriptor);
    ui::push_ui_input_text(1, 0, 0xFFFFFFFF, &box->input_text);
}
#endif

void list_t::submit() {
    ui::mark_ui_textured_section(get_game_font()->font_img.descriptor);
    ui::push_color_box(&list_box);

    for (uint32_t i = 0; i < button_count; ++i) {
        ui::push_text(&right_buttons[i].text);
        ui::push_color_box(&right_buttons[i].box);
    }

    ui::push_color_box(&typing_box.box);
    ui::mark_ui_textured_section(get_game_font()->font_img.descriptor);
    ui::push_ui_input_text(1, 0, 0xFFFFFFFF, &typing_box.input_text);

    for (uint32_t i = 0; i < item_count; ++i) {
        ui::push_color_box(&items[i].box);
        ui::push_text(&items[i].text);
    }
}

void list_t::input(const app::raw_input_t *input) {
    { // For the typing space
        bool hovered_over_ip_address = is_hovering_over_box(&typing_box.box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
        color_pair_t pair = typing_box.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over_ip_address);

        typing_box.box.color = pair.current_background;

        if (input->buttons[app::BT_MOUSE_LEFT].instant && hovered_over_ip_address) {
            typing_box.is_typing = 1;

            selected_item = 0xFFFF;
        }

        if (typing_box.is_typing)
            typing_box.input_text.input(input);
    }

    { // For the buttons to the right of the screen
        for (uint32_t i = 0; i < button_count; ++i) {
            list_button_t *button = &right_buttons[i];

            bool hovered_over = is_hovering_over_box(&button->box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
            color_pair_t pair = button->color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over);
            button->box.color = pair.current_background;

            if (hovered_over && input->buttons[app::BT_MOUSE_LEFT].instant) {
                button->handle_input_proc(this);
            }
        }
    }

    { // For the selection of list items
        for (uint32_t i = 0; i < item_count; ++i) {
            list_item_t *item = &items[i];

            if (i == selected_item) {
                item->box.color = MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR;
            }
            else {
                bool hovered_over = is_hovering_over_box(&item->box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
                color_pair_t pair = item->button_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over);
                item->box.color = pair.current_background;

                if (hovered_over && input->buttons[app::BT_MOUSE_LEFT].instant) {
                    selected_item = i;

                    typing_box.is_typing = 0;
                }
            }
        }
    }
}

}
