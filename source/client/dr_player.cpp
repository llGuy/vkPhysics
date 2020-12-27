#include "dr_player.hpp"
#include <vkph_player.hpp>
#include <common/allocators.hpp>

player_render_t *dr_player_render_init() {
    player_render_t *player_render = FL_MALLOC(player_render_t, 1);
    memset(player_render, 0, sizeof(player_render_t));
    player_render->rolling_matrix = matrix4_t(1.0f);
    return player_render;
}

bool dr_is_animation_repeating(vkph::player_animated_state_t state) {
    static const bool map[] = {1, 1, 1, 0, 0, 1, 1, 0, 1, 1};
    
    return map[(uint32_t)state];
}
