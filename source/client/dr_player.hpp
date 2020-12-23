#pragma once

#include <common/player.hpp>
#include <vk.hpp>

typedef vk::mesh_render_data_t player_render_data_t;

struct player_render_t {
    player_render_data_t render_data;
    vk::animated_instance_t animations;
    float rotation_speed;
    float rotation_angle;
    matrix4_t rolling_matrix;
    float shape_animation_time;

    struct {
        uint8_t repeat: 1;
    };
};

player_render_t *dr_player_render_init();
bool dr_is_animation_repeating(player_animated_state_t state);
