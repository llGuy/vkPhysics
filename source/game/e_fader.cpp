#include "engine.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>
#include <common/allocators.hpp>

static smooth_linear_interpolation_t current_screen_brightness;

static event_begin_fade_effect_t data;

void e_fader_init() {
    current_screen_brightness.in_animation = 0;
    current_screen_brightness.current = 0.0f;
    current_screen_brightness.prev = 0.0f;
    current_screen_brightness.next = 0.0f;
    current_screen_brightness.current_time = 0.0f;
    current_screen_brightness.in_animation = 0;

    data = {};
}

void e_begin_fade_effect(
    event_begin_fade_effect_t *idata) {
    current_screen_brightness.set(
        1,
        current_screen_brightness.current,
        idata->dest_value,
        idata->duration);

    data = *idata;
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

            for (uint32_t i = 0; i < data.trigger_count; ++i) {
                submit_event(
                    data.triggers[i].trigger_type,
                    data.triggers[i].next_event_data,
                    events);
            }

            if (data.fade_back) {
                // Fade back in
                event_begin_fade_effect_t *fade_back_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                fade_back_data->dest_value = 1.0f;
                fade_back_data->duration = duration;
                fade_back_data->trigger_count = 0;
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
