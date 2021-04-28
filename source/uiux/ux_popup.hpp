#pragma once

#include "ui_box.hpp"
#include "ui_font.hpp"
#include "ux_menu_layout.hpp"

namespace ux {

enum popup_section_type_t { PST_BUTTON_DOUBLE, PST_BUTTON_SINGLE, PST_TEXT, PST_INPUT, PST_INVALID };

struct popup_button_t {
    ui::box_t box;
    ui::text_t text;
    widget_color_t color;
    const char *button_string;
    void (* handle_press_proc)(struct popup_t *);

    popup_button_t() = default;
};

struct popup_section_t {
    popup_section_type_t type;

    struct {
        popup_button_t buttons[2];
    } d_button;

    struct {
        popup_button_t button;
    } s_button;

    struct {
        ui::text_t text;
        // Invisible
        ui::box_t box;
        const char *string;
    } text;

    struct {
        ui::box_t box;
        ui::input_text_t input;
        widget_color_t color;
    } input;

    popup_section_t() = default;
};

#define MAX_SECTIONS_IN_POPUP 8

struct popup_t {
    // Section in which the input (that the user is currently typing in) is contained
    uint32_t current_typing_section;
    // Used to pass parameters
    uint32_t dummy;

    ui::box_t panel;

    uint32_t vertical_section_count;
    popup_section_t sections[MAX_SECTIONS_IN_POPUP];
};

void init_popups();
// Pushes a popup to the ui stack and returns a pointer to continue initialisation
popup_t *add_popup(uint32_t vertical_section_count);
void push_popup_section_button_double(
    popup_t *popup,
    const char **button_text,
    void (** handle_press_proc)(popup_t *));
void push_popup_section_button_single(
    popup_t *popup,
    const char *button_text,
    void (* handle_press_proc)(popup_t *));
void push_popup_section_text(popup_t *popup, const char *text);
void push_popup_section_input(popup_t *popup);
// Initialise all the box_t and text_t, etc...
void prepare_popup_for_render(popup_t *popup);
void submit_popups();
void popup_input(const app::raw_input_t *input);

}
