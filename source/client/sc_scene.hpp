#pragma once

#include <vk.hpp>

enum scene_type_t {
    ST_MAIN_MENU,
    ST_GAME_PLAY,
    ST_MAP_CREATOR,
    ST_INVALID
};

void sc_scenes_init(struct event_submissions_t *events);
void sc_bind(scene_type_t type);
void sc_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer shadow,
    struct event_submissions_t *events);
vk::eye_3d_info_t *sc_get_eye_info();
vk::lighting_info_t *sc_get_lighting_info();
