#include <common/event.hpp>
#include "e_internal.hpp"
#include "renderer/renderer.hpp"
#include <common/allocators.hpp>

void e_subscribe_to_events(
    listener_t game_core_listener,
    event_submissions_t *events) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener, events);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, game_core_listener, events);
    subscribe_to_event(ET_BEGIN_FADE, game_core_listener, events);
    subscribe_to_event(ET_FADE_FINISHED, game_core_listener, events);
    subscribe_to_event(ET_LAUNCH_MAIN_MENU_SCREEN, game_core_listener, events);
    subscribe_to_event(ET_EXIT_MAIN_MENU_SCREEN, game_core_listener, events);
    subscribe_to_event(ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY, game_core_listener, events);
    subscribe_to_event(ET_LAUNCH_INGAME_MENU, game_core_listener, events);
    subscribe_to_event(ET_LAUNCH_GAME_MENU_SCREEN, game_core_listener, events);
    subscribe_to_event(ET_BEGIN_AI_TRAINING, game_core_listener, events);
}

void e_game_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    switch(event->type) {
        
    case ET_CLOSED_WINDOW: {
        e_terminate();
    } break;

    case ET_RESIZE_SURFACE: {
        event_surface_resize_t *data = (event_surface_resize_t *)event->data;

        handle_resize(data->width, data->height);

        FL_FREE(data);
    } break;

    case ET_BEGIN_FADE: {
        event_begin_fade_effect_t *data = (event_begin_fade_effect_t *)event->data;
        
        e_begin_fade_effect(data);

        FL_FREE(data);
    } break;

    case ET_PRESSED_ESCAPE: {
    } break;

    case ET_LAUNCH_MAIN_MENU_SCREEN: {
        e_begin_startup_world_rendering();
        e_enter_ui_screen();
        e_begin_blurred_rendering();
    } break;

    case ET_LAUNCH_GAME_MENU_SCREEN: {
        e_end_startup_world_rendering();
        e_enter_ui_screen();
        e_begin_crisp_rendering();
    } break;

    case ET_EXIT_MAIN_MENU_SCREEN: {
        e_end_startup_world_rendering();
        e_begin_crisp_rendering();
    } break;

    case ET_BEGIN_AI_TRAINING: {
        e_begin_crisp_rendering();
    } break;

    case ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY: {
        e_end_startup_world_rendering();
        e_enter_world();
    } break;

    case ET_LAUNCH_INGAME_MENU: {
        e_enter_ui_screen();
    } break;
        
    }
}

