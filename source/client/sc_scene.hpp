#pragma once

#include <renderer/renderer.hpp>

enum scene_type_t {
    ST_MAIN_MENU,
    ST_GAME_PLAY,
    ST_INVALID
};

void sc_scenes_init(struct event_submissions_t *events);
void sc_bind(scene_type_t type);
void sc_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, struct event_submissions_t *events);
eye_3d_info_t *sc_get_eye_info();
lighting_info_t *sc_get_lighting_info();
