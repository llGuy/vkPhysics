#pragma once

#include <renderer/renderer.hpp>

typedef mesh_render_data_t player_render_data_t;

enum player_animated_state_t {
    PAS_IDLE,
    PAS_WALKING,
    PAS_RUNNING,
    PAS_JUMPING_UP,
    PAS_JUMPING_DOWN,
    PAS_LEFT_WALKING,
    PAS_RIGHT_WALKING,
    PAS_STOP_FAST,
    PAS_BACKWALKING,
    PAS_TRIPPING
};

struct player_render_t {
    player_render_data_t render_data;
    animated_instance_t animations;
    float rotation_speed;
    float rotation_angle;
    matrix4_t rolling_matrix;
    float shape_animation_time;

    struct {
        uint8_t animated_state: 7;
        uint8_t repeat: 1;
    };
};

player_render_t *dr_player_render_init();
bool dr_is_animation_repeating(player_animated_state_t state);
