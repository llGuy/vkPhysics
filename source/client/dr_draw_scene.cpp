#include <vkph_state.hpp>
#include <vkph_team.hpp>
#include <vkph_physics.hpp>
#include "dr_rsc.hpp"
#include "cl_main.hpp"
#include "dr_chunk.hpp"
#include "dr_player.hpp"
#include "ux_scene.hpp"
#include "wd_predict.hpp"
#include "dr_draw_scene.hpp"
#include <app.hpp>
#include <vkph_player.hpp>
#include <vk.hpp>


void dr_draw_game(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer shadow,
    vkph::state_t *state) {
    ux::scene_info_t *scene_info = ux::get_scene_info();
    const vk::eye_3d_info_t *eye_info = &scene_info->eye;
    vk::swapchain_information_t swapchain_info = vk::get_swapchain_info();

    float aspect_ratio = (float)swapchain_info.width / (float)swapchain_info.height;

#if 0
    // Get view frustum for culling
    frustum_t frustum = {};
    create_frustum(
        &frustum,
        eye_info->position,
        eye_info->direction,
        eye_info->up,
        glm::radians(eye_info->fov / 2.0f),
        aspect_ratio,
        eye_info->near,
        eye_info->far);
#endif

    s_players_gpu_sync_and_render(render, shadow, transfer, state);
    s_chunks_gpu_sync_and_render(NULL, render, shadow, transfer, state);
    s_projectiles_gpu_sync_and_render(render, shadow, transfer, state);

    if (render != VK_NULL_HANDLE) {
        vk::render_environment(render);
    }
}

void dr_draw_premade_scene(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    fixed_premade_scene_t *scene) {
    vk::begin_mesh_submission(
        render,
        dr_get_shader_rsc(GS_BALL),
        dr_chunk_colors_g.chunk_color_set);

    scene->world_render_data.color = vector4_t(0.0f);

    vk::submit_mesh(
        render,
        &scene->world_mesh,
        dr_get_shader_rsc(GS_BALL),
        {&scene->world_render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
}
