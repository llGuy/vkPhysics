#include "dr_player.hpp"
#include <common/player.hpp>
#include <common/allocators.hpp>

player_render_t *dr_player_render_init() {
    return FL_MALLOC(player_render_t, 1);
}

bool dr_is_animation_repeating(player_animated_state_t state) {
    static const bool map[] = {1, 1, 1, 0, 0, 1, 1, 0, 1, 1};
    
    return map[(uint32_t)state];
}
