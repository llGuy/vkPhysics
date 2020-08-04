#include "cl_main.hpp"
#include "client/cl_view.hpp"
#include "fx_fade.hpp"
#include "cl_view.hpp"
#include <common/event.hpp>
#include <common/allocators.hpp>
#include <renderer/renderer.hpp>

void cl_subscribe_to_events(
    listener_t game_core_listener,
    event_submissions_t *events) {
    subscribe_to_event(ET_CLOSED_WINDOW, game_core_listener, events);
    subscribe_to_event(ET_RESIZE_SURFACE, game_core_listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, game_core_listener, events);
    subscribe_to_event(ET_BEGIN_FADE, game_core_listener, events);
    subscribe_to_event(ET_FADE_FINISHED, game_core_listener, events);
    subscribe_to_event(ET_BEGIN_AI_TRAINING, game_core_listener, events);
}

void cl_game_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    switch(event->type) {
        
    case ET_CLOSED_WINDOW: {
        cl_terminate();
    } break;

    case ET_RESIZE_SURFACE: {
        event_surface_resize_t *data = (event_surface_resize_t *)event->data;

        handle_resize(data->width, data->height);

        FL_FREE(data);
    } break;

    case ET_BEGIN_FADE: {
        event_begin_fade_effect_t *data = (event_begin_fade_effect_t *)event->data;
        
        fx_begin_fade_effect(data);

        FL_FREE(data);
    } break;

    case ET_PRESSED_ESCAPE: {
    } break;

    // case ET_LAUNCH_MAIN_MENU_SCREEN: {
    //     cl_change_view_type(GVT_MENU);
    // } break;

    // case ET_LAUNCH_GAME_MENU_SCREEN: {
    //     cl_change_view_type(GVT_IN_GAME);
    // } break;

        // TODO: See how the game works with this
    // case ET_EXIT_MAIN_MENU_SCREEN: {
    //     cl_end_startup_world_rendering();
    //     cl_begin_crisp_rendering();
    // } break;

    case ET_BEGIN_AI_TRAINING: {
        // cl_begin_crisp_rendering();
    } break;

    // case ET_CLEAR_MENUS_AND_ENTER_GAMEPLAY: {
    //     cl_change_view_type(GVT_IN_GAME);
    // } break;

    // case ET_LAUNCH_INGAME_MENU: {
    //     cl_change_view_type(GVT_MENU);
    // } break;

    default: {
    } break;
        
    }
}

