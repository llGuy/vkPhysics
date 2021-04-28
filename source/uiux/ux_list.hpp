#pragma once

#include "ui_box.hpp"
#include "ui_font.hpp"
#include "ux_hover.hpp"

namespace ux {

struct list_button_t {
    ui::box_t box;
    ui::text_t text;
    widget_color_t color;

    void (* handle_input_proc)(struct list_t *list);
};

struct list_item_t {
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

struct list_t {
    // On the right of the right of the list, there will a number of buttons to press
    list_button_t *right_buttons;
    uint32_t button_count;

    // On the left side of the list, there will be a little box to type something
    typing_box_t typing_box;

    // Filling up most of the list, there will be the actual list where you can click on items
    ui::box_t list_box;
    uint32_t item_count;
    list_item_t *items;
    uint32_t selected_item = 0xFFFF;
    // This proc sets the string inside the list item
    void (* fill_item_proc)(list_item_t *);



    void init(
        ui::box_t *parent,
        uint32_t right_buttons_count,
        const char **right_button_text,
        void (** handle_input_procs)(list_t *),
        void (* fill_item_proc)(list_item_t *));

    // Clears all the items in a list
    void clear();

    // Allocates enough memory for the list
    void begin(uint32_t count);

    // Adds an item to the list
    void add(const void *data);

    // Initialises all of the items in the list and all the rendering data
    void end();

    // Submits a list for rendering
    void submit();

    void input(const app::raw_input_t *raw_input);
};

}
