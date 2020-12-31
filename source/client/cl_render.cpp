#include <ux_scene.hpp>
#include "cl_render.hpp"

namespace cl {

void draw_game(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
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

    players_gpu_sync_and_render(cmdbufs->render_cmdbuf, cmdbufs->render_shadow_cmdbuf, cmdbufs->transfer_cmdbuf, state);
    chunks_gpu_sync_and_render(NULL, cmdbufs->render_cmdbuf, cmdbufs->render_shadow_cmdbuf, cmdbufs->transfer_cmdbuf, state);
    projectiles_gpu_sync_and_render(cmdbufs->render_cmdbuf, cmdbufs->render_shadow_cmdbuf, cmdbufs->transfer_cmdbuf, state);

    if (cmdbufs->render_cmdbuf != VK_NULL_HANDLE) {
        vk::render_environment(cmdbufs->render_cmdbuf);
    }
}

void init_render_resources() {
    init_chunk_render_resources();
    init_player_render_resources();
    init_projectile_render_resources();
    init_fixed_premade_scene_shaders();
}

}
