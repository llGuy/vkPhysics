#include "ui.hpp"
#include "u_internal.hpp"
#include <common/event.hpp>
#include <renderer/input.hpp>

enum ui_stack_item_t {
    USI_MAIN_MENU,
    USI_HUD,
    USI_INVALID,
};

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static ui_stack_item_t stack_items[10];
static int32_t stack_item_count;

static void s_ui_event_listener(
    void *object,
    event_t *event) {
    // ...
    switch(event->type) {

    case ET_LAUNCH_MAIN_MENU_SCREEN: {
        stack_items[0] = USI_MAIN_MENU;
        stack_item_count = 1;
    } break;

    default: {
    } break;

    }
}

static listener_t ui_listener;

void ui_init(
    event_submissions_t *events) {
    stack_item_count = 0;

    ui_listener = set_listener_callback(
        &s_ui_event_listener,
        NULL,
        events);

    subscribe_to_event(
        ET_LAUNCH_MAIN_MENU_SCREEN,
        ui_listener,
        events);

    u_main_menu_init();

    u_hud_init();
}

void tick_ui() {
    game_input_t *game_input = get_game_input();

    if (stack_item_count) {
        ui_stack_item_t last = stack_items[stack_item_count - 1];

        switch (last) {
        case USI_MAIN_MENU: {
            u_main_menu_input(game_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }

    for (uint32_t i = 0; i < stack_item_count; ++i) {
        switch (stack_items[i]) {
        case USI_MAIN_MENU: {
            u_submit_main_menu();
        } break;

        case USI_HUD: {
            u_submit_hud();
        } break;
        }
    }
}
