#include "engine.hpp"
#include <common/log.hpp>
#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>

static smooth_linear_interpolation_t current_screen_brightness;

void e_fader_init() {
    current_screen_brightness.in_animation = 0;
    current_screen_brightness.current = 0.0f;
    current_screen_brightness.prev = 0.0f;
    current_screen_brightness.next = 0.0f;
    current_screen_brightness.current_time = 0.0f;
    current_screen_brightness.in_animation = 0;
}

void e_begin_fade_effect(
    float effect_duration,
    float destination_value) {

    current_screen_brightness.set(
        1,
        current_screen_brightness.current,
        destination_value,
        effect_duration);
}

void e_tick_fade_effect(
    event_submissions_t *events) {
    if (current_screen_brightness.in_animation) {
        current_screen_brightness.animate(
            logic_delta_time());

        if (!current_screen_brightness.in_animation) {
            submit_event(
                ET_FADE_FINISHED,
                NULL,
                events);
        }
    }

    set_main_screen_brightness(
      current_screen_brightness.current);
}
