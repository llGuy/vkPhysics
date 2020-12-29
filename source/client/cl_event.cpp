#include "cl_main.hpp"
#include "cl_view.hpp"
#include "fx_fade.hpp"
#include "cl_view.hpp"
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <allocators.hpp>

#include <vk.hpp>

void cl_subscribe_to_events(
    vkph::listener_t game_core_listener) {
    vkph::subscribe_to_event(vkph::ET_CLOSED_WINDOW, game_core_listener);
    vkph::subscribe_to_event(vkph::ET_RESIZE_SURFACE, game_core_listener);
    vkph::subscribe_to_event(vkph::ET_BEGIN_FADE, game_core_listener);
}

void cl_game_event_listener(
    void *object,
    vkph::event_t *event) {
    switch(event->type) {
        
    case vkph::ET_CLOSED_WINDOW: {
        cl_terminate();
    } break;

    case vkph::ET_RESIZE_SURFACE: {
        auto *data = (vkph::event_surface_resize_t *)event->data;

        vk::resize_render_pipeline(data->width, data->height);

        FL_FREE(data);
    } break;

    case vkph::ET_BEGIN_FADE: {
        auto *data = (vkph::event_begin_fade_effect_t *)event->data;
        
        fx_begin_fade_effect(data);

        FL_FREE(data);
    } break;

    default: {
    } break;
        
    }
}

