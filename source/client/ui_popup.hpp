#pragma once

#include "ui_hover.hpp"
#include <common/event.hpp>
#include "renderer/renderer.hpp"

enum popup_section_type_t { PST_BUTTON_DOUBLE, PST_BUTTON_SINGLE, PST_TEXT, PST_INPUT, PST_INVALID };

struct popup_button_t {
    ui_box_t box;
    ui_text_t text;
    widget_color_t color;
    const char *button_string;
    void (* handle_press_proc)(struct ui_popup_t *, event_submissions_t *);

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
        ui_text_t text;
        // Invisible
        ui_box_t box;
        const char *string;
    } text;

    struct {
        ui_box_t box;
        ui_input_text_t input;
        widget_color_t color;
    } input;

    popup_section_t() = default;
};

#define MAX_SECTIONS_IN_POPUP 8

struct ui_popup_t {
    // Section in which the input (that the user is currently typing in) is contained
    uint32_t current_typing_section;
    // Used to pass parameters
    uint32_t dummy;

    ui_box_t panel;

    uint32_t vertical_section_count;
    popup_section_t sections[MAX_SECTIONS_IN_POPUP];
};

void ui_popups_init();
// Pushes a popup to the ui stack and returns a pointer to continue initialisation
ui_popup_t *ui_add_popup(uint32_t vertical_section_count);
void ui_push_popup_section_button_double(
    ui_popup_t *popup,
    const char **button_text,
    void (** handle_press_proc)(ui_popup_t *, event_submissions_t *));
void ui_push_popup_section_button_single(
    ui_popup_t *popup,
    const char *button_text,
    void (* handle_press_proc)(ui_popup_t *, event_submissions_t *));
void ui_push_popup_section_text(ui_popup_t *popup, const char *text);
void ui_push_popup_section_input(ui_popup_t *popup);
// Initialise all the ui_box_t and ui_text_t, etc...
void ui_prepare_popup_for_render(ui_popup_t *popup);
void ui_submit_popups();
void ui_popup_input(event_submissions_t *events, raw_input_t *input);
