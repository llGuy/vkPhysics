#pragma once

#include <vkph_events.hpp>

namespace cl {

void subscribe_to_game_events(vkph::listener_t world_listener);
void game_event_listener(void *object, vkph::event_t *event);

}
