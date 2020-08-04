#pragma once

#include <common/event.hpp>

void fx_fader_init();
void fx_begin_fade_effect(event_begin_fade_effect_t *idata);
void fx_tick_fade_effect(event_submissions_t *events);
