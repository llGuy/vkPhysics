#include <vk.hpp>
#include <math.hpp>
#include "cl_main.hpp"
#include <vkph_events.hpp>
#include "cl_scene_transition.hpp"

namespace cl {

static linear_interpolation_f32_t current_screen_brightness;
static vkph::event_begin_fade_effect_t data;

void init_scene_transition() {
    current_screen_brightness.in_animation = 0;
    current_screen_brightness.current = 0.0f;
    current_screen_brightness.prev = 0.0f;
    current_screen_brightness.next = 0.0f;
    current_screen_brightness.current_time = 0.0f;
    current_screen_brightness.in_animation = 0;

    data = {};
}

void begin_scene_transition(vkph::event_begin_fade_effect_t *idata) {
    current_screen_brightness.set(
        1,
        current_screen_brightness.current,
        idata->dest_value,
        idata->duration);

    data = *idata;
}

void tick_scene_transition() {
    float duration = current_screen_brightness.max_time;

    if (current_screen_brightness.in_animation) {
        current_screen_brightness.animate(delta_time());

        if (!current_screen_brightness.in_animation) {
            vkph::submit_event(vkph::ET_FADE_FINISHED, NULL);

            for (uint32_t i = 0; i < data.trigger_count; ++i) {
                submit_event(data.triggers[i].trigger_type, data.triggers[i].next_event_data);
            }

            if (data.fade_back) {
                // Fade back in
                auto *fade_back_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
                fade_back_data->dest_value = 1.0f;
                fade_back_data->duration = duration;
                fade_back_data->trigger_count = 0;
                fade_back_data->fade_back = 0;

                vkph::submit_event(vkph::ET_BEGIN_FADE, fade_back_data);
            }
        }
    }

    vk::set_main_screen_brightness(current_screen_brightness.current);
}

}
