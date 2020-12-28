#pragma once

#include "ui_core.hpp"
#include "ui_menu_layout.hpp"

#include <ui.hpp>
#include <app.hpp>

struct ui_list_button_t {
    ui::box_t box;
    ui::text_t text;
    widget_color_t color;

    void (* handle_input_proc)(struct ui_list_t *list);
};

struct ui_list_item_t {
    ui::box_t box;
    ui::text_t text;
    widget_color_t button_color;
    widget_color_t color;

    const void *data;
};

struct typing_box_t {
    bool is_typing;
    ui::box_t box;
    ui::input_text_t input_text;
    widget_color_t color;
};

void ui_submit_typing_box(typing_box_t *box);

struct ui_list_t {
    // On the right of the right of the list, there will a number of buttons to press
    ui_list_button_t *right_buttons;
    uint32_t button_count;

    // On the left side of the list, there will be a little box to type something
    typing_box_t typing_box;

    // Filling up most of the list, there will be the actual list where you can click on items
    ui::box_t list_box;
    uint32_t item_count;
    ui_list_item_t *items;
    uint32_t selected_item = 0xFFFF;
    // This proc sets the string inside the list item
    void (* fill_item_proc)(ui_list_item_t *);
};

void ui_list_init(
    ui::box_t *parent,
    ui_list_t *list,
    uint32_t right_buttons_count,
    const char **right_button_text,
    void (** handle_input_procs)(ui_list_t *),
    void (* fill_item_proc)(ui_list_item_t *));
// Clears all the items in a list
void ui_list_clear(ui_list_t *list);
// Allocates enough memory for the list
void ui_list_begin(ui_list_t *list, uint32_t count);
// Adds an item to the list
void ui_list_add(ui_list_t *list, const void *data);
// Initialises all of the items in the list and all the rendering data
void ui_list_end(ui_list_t *list);
// Submits a list for rendering
void ui_submit_list(ui_list_t *list);
void ui_list_input(ui_list_t *list, const app::raw_input_t *raw_input);
