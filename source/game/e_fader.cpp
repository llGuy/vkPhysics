#include "engine.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>
#include <common/allocators.hpp>

static smooth_linear_interpolation_t current_screen_brightness;

static bool trigger_another_event = 0;
static event_type_t to_trigger = ET_INVALID_EVENT_TYPE;
static void *next_event_data = NULL;
static bool fade_back = 0;

void e_fader_init() {
    current_screen_brightness.in_animation = 0;
    current_screen_brightness.current = 0.0f;
    current_screen_brightness.prev = 0.0f;
    current_screen_brightness.next = 0.0f;
    current_screen_brightness.current_time = 0.0f;
    current_screen_brightness.in_animation = 0;

    trigger_another_event = 0;
    to_trigger = ET_INVALID_EVENT_TYPE;
    next_event_data = NULL;
    fade_back = 0;
}

void e_begin_fade_effect(
    event_begin_fade_effect_t *data) {
    current_screen_brightness.set(
        1,
        current_screen_brightness.current,
        data->dest_value,
        data->duration);

    trigger_another_event = data->trigger_another_event;
    if (trigger_another_event) {
        to_trigger = data->to_trigger;
        next_event_data = data->next_event_data;
    }
    else {
        to_trigger = ET_INVALID_EVENT_TYPE;
        next_event_data = NULL;
    }

    fade_back = data->fade_back;
}

void e_tick_fade_effect(
    event_submissions_t *events) {
    float duration = current_screen_brightness.max_time;

    if (current_screen_brightness.in_animation) {
        current_screen_brightness.animate(
            logic_delta_time());

        if (!current_screen_brightness.in_animation) {
            submit_event(
                ET_FADE_FINISHED,
                NULL,
                events);

            if (trigger_another_event) {
                submit_event(
                    to_trigger,
                    next_event_data,
                    events);
            }

            if (fade_back) {
                // Fade back in
                event_begin_fade_effect_t *fade_back_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                fade_back_data->dest_value = 1.0f;
                fade_back_data->duration = duration;
                fade_back_data->trigger_another_event = 0;
                fade_back_data->fade_back = 0;

                submit_event(
                    ET_BEGIN_FADE,
                    fade_back_data,
                    events);
            }
        }
    }

    set_main_screen_brightness(
      current_screen_brightness.current);
}
