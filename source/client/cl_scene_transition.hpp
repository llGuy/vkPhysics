#pragma once

#include <vkph_event_data.hpp>

namespace cl {

/* 
  The transition between scenes consists in a simple fade effect for now.
  This function merely just initialises the data necessary to get these
  transitions going (such as initial fade brightness, etc...).
 */
void init_scene_transition();
void begin_scene_transition(vkph::event_begin_fade_effect_t *idata);
void tick_scene_transition();

}
