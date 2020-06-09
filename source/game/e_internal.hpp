#pragma once

void e_fader_init();

void e_begin_fade_effect(
    struct event_begin_fade_effect_t *data);   // destination value should be 1 if fade in or 0 if fade out

void e_tick_fade_effect(
    struct event_submissions_t *events);
