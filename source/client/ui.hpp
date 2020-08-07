#pragma once

enum ui_stack_item_t {
    USI_MAIN_MENU,
    USI_HUD,
    USI_GAME_MENU,
    USI_INVALID,
};

void ui_init(struct event_submissions_t *events);
void handle_ui_input(event_submissions_t *events);
void tick_ui(struct event_submissions_t *events);

void push_ui_panel(ui_stack_item_t item);
void pop_ui_panel();
void clear_ui_panels();
