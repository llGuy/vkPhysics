#include "ui_list.hpp"
#include <renderer/input.hpp>
#include "renderer/renderer.hpp"
#include <common/allocators.hpp>

void ui_list_init(
    ui_box_t *parent,
    ui_list_t *list,
    uint32_t right_buttons_count,
    const char **right_button_text,
    void (** handle_input_procs)(ui_list_t *, event_submissions_t *),
    void (* fill_item_proc)(ui_list_item_t *item)) {
    { // Initialise the list box
        list->list_box.init(
            RT_RIGHT_DOWN,
            2.1f,
            ui_vector2_t(-0.02f, 0.15f),
            ui_vector2_t(0.95f, 0.95f), parent, 0x09090936);
    }

    { // Initialise the typing box
        list->typing_box.box.init(
            RT_LEFT_DOWN,
            6.0f,
            ui_vector2_t(0.02f, 0.03f),
            ui_vector2_t(0.28f, 0.2f),
            parent,
            0x09090936);

        list->typing_box.input_text.text.init(
            &list->typing_box.box,
            ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.9f,
            18,
            1.8f);

        list->typing_box.color.init(
            0x09090936,
            MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
            0xFFFFFFFF,
            0xFFFFFFFF);

        list->typing_box.input_text.text_color = 0xFFFFFFFF;
        list->typing_box.input_text.cursor_position = 0;
        list->typing_box.is_typing = 0;
    }

    { // Set default values for the list items
        list->item_count = 0;
        list->items = NULL;
        list->fill_item_proc = fill_item_proc;
    }

    { // Initialise the buttons on the right of the list
        float dx = 0.23f;
        list->right_buttons = FL_MALLOC(ui_list_button_t, right_buttons_count);
        list->button_count = right_buttons_count;

        for (uint32_t i = 0; i < right_buttons_count; ++i) {
            ui_list_button_t *button = &list->right_buttons[i];

            button->handle_input_proc = handle_input_procs[i];

            button->box.init(
                RT_RIGHT_DOWN,
                4.0f,
                ui_vector2_t(-0.02f - ((float)i * dx), 0.03f),
                ui_vector2_t(0.2f, 0.2f),
                parent,
                0x09090936);

            button->text.init(
                &button->box,
                ui_game_font(),
                ui_text_t::font_stream_box_relative_to_t::BOTTOM,
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

void ui_list_clear(ui_list_t *list) {
    if (list->items) {
        list->item_count = 0;
        FL_FREE(list->items);
    }
}

void ui_list_begin(ui_list_t *list, uint32_t count) {
    list->item_count = 0;
    list->items = FL_MALLOC(ui_list_item_t, count);
}

void ui_list_add(ui_list_t *list, void *data) {
    list->items[list->item_count].data = data;
    list->item_count++;
}

void ui_list_end(ui_list_t *list) {
    float server_button_height = 0.1f;

    for (uint32_t i = 0; i < list->item_count; ++i) {
        ui_list_item_t *item = &list->items[i];

        item->box.init(
            RT_LEFT_UP,
            20.8f,
            ui_vector2_t(0.0f, -server_button_height * (float)i),
            ui_vector2_t(1.0f, server_button_height),
            &list->list_box,
            0x05050536);

        item->text.init(
            &item->box,
            ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.8f,
            60,
            1.8f);

        list->fill_item_proc(item);

        item->button_color.init(
            0x05050536,
            MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
            0xFFFFFFFF,
            0xFFFFFFFF);
    }
}

void ui_submit_typing_box(typing_box_t *box) {
    push_color_ui_box(&box->box);
    mark_ui_textured_section(ui_game_font()->font_img.descriptor);
    push_ui_input_text(1, 0, 0xFFFFFFFF, &box->input_text);
}

void ui_submit_list(ui_list_t *list) {
    mark_ui_textured_section(ui_game_font()->font_img.descriptor);
    push_color_ui_box(&list->list_box);

    for (uint32_t i = 0; i < list->button_count; ++i) {
        push_ui_text(&list->right_buttons[i].text);
        push_color_ui_box(&list->right_buttons[i].box);
    }

    ui_submit_typing_box(&list->typing_box);

    for (uint32_t i = 0; i < list->item_count; ++i) {
        push_color_ui_box(&list->items[i].box);
        push_ui_text(&list->items[i].text);
    }
}

void ui_list_input(ui_list_t *list, event_submissions_t *events, raw_input_t *input) {
    { // For the typing space
        bool hovered_over_ip_address = ui_hover_over_box(&list->typing_box.box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
        color_pair_t pair = list->typing_box.color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over_ip_address);

        list->typing_box.box.color = pair.current_background;

        if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_ip_address) {
            list->typing_box.is_typing = 1;

            list->selected_item = 0xFFFF;
        }

        if (list->typing_box.is_typing)
            list->typing_box.input_text.input(input);
    }

    { // For the buttons to the right of the screen
        for (uint32_t i = 0; i < list->button_count; ++i) {
            ui_list_button_t *button = &list->right_buttons[i];

            bool hovered_over = ui_hover_over_box(&button->box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
            color_pair_t pair = button->color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over);
            button->box.color = pair.current_background;

            if (hovered_over && input->buttons[BT_MOUSE_LEFT].instant) {
                button->handle_input_proc(list, events);
            }
        }
    }

    { // For the selection of list items
        for (uint32_t i = 0; i < list->item_count; ++i) {
            ui_list_item_t *item = &list->items[i];

            if (i == list->selected_item) {
                item->box.color = MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR;
            }
            else {
                bool hovered_over = ui_hover_over_box(&item->box, vector2_t(input->cursor_pos_x, input->cursor_pos_y));
                color_pair_t pair = item->button_color.update(MENU_WIDGET_HOVER_COLOR_FADE_SPEED, hovered_over);
                item->box.color = pair.current_background;

                if (hovered_over && input->buttons[BT_MOUSE_LEFT].instant) {
                    list->selected_item = i;

                    list->typing_box.is_typing = 0;
                }
            }
        }
    }
}
