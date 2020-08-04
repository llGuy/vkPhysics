#pragma once

#include <renderer/renderer.hpp>

enum game_mode_type_t {
    GMT_MAIN_MENU,
    GMT_GAME_PLAY,
    GMT_INVALID
};

void gm_modes_init(struct event_submissions_t *events);
void gm_bind(game_mode_type_t mode);
void gm_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, struct event_submissions_t *events);
eye_3d_info_t *gm_get_eye_info();
lighting_info_t *gm_get_lighting_info();
