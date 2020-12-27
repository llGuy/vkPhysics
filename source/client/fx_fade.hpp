#pragma once

#include <vkph_event_data.hpp>

void fx_fader_init();
void fx_begin_fade_effect(vkph::event_begin_fade_effect_t *idata);
void fx_tick_fade_effect();
