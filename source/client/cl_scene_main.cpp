#include "cl_frame.hpp"
#include "cl_scene.hpp"
#include <ux.hpp>
#include "cl_render.hpp"
#include "cl_main.hpp"
#include "nw_client.hpp"
#include "ux_scene.hpp"
#include "wd_spectate.hpp"
#include <vkph_events.hpp>
#include <ux_menu_main.hpp>
#include <vkph_player.hpp>
#include <cstddef>
#include <app.hpp>
#include <vk.hpp>
#include <ui_submit.hpp>
#include <vulkan/vulkan_core.h>

namespace cl {

void main_scene_t::init() {
    structure_.load("assets/misc/startup/default.startup");
}

void main_scene_t::subscribe_to_events(vkph::listener_t listener) {
    vkph::subscribe_to_event(vkph::ET_ENTER_MAIN_MENU_SCENE, listener);
    vkph::subscribe_to_event(vkph::ET_REQUEST_TO_JOIN_SERVER, listener);
    vkph::subscribe_to_event(vkph::ET_REQUEST_USER_INFORMATION, listener);
    vkph::subscribe_to_event(vkph::ET_ENTER_MAP_CREATOR_SCENE, listener);
}

void main_scene_t::prepare_for_binding(vkph::state_t *state) {
    ux::clear_main_menu();

    vkph::player_t *spect = wd_get_spectator();

    // Position the player at the right place and view in the right direction
    spect->ws_position = structure_.position;
    spect->ws_view_direction = structure_.view_direction;
    spect->ws_up_vector = structure_.up_vector;

    get_frame_info()->blurred = 1;
    get_frame_info()->ssao = 0;
}

void main_scene_t::prepare_for_unbinding(vkph::state_t *state) {
    ux::clear_panels();
}

void main_scene_t::handle_input(vkph::state_t *state) {
    ux::handle_input(state);

    vkph::player_t *spect = wd_get_spectator();
    const app::game_input_t *game_input = app::get_game_input();

    static bool rotating = 0;
    static vector3_t dest = vector3_t(0.0f);

    float dmouse_x = game_input->mouse_x - game_input->previous_mouse_x;
    float dmouse_y = game_input->mouse_y - game_input->previous_mouse_y;

    if (rotating) {
        vector3_t diff = dest - spect->ws_view_direction;
        if (glm::dot(diff, diff) > 0.00001f) {
            spect->ws_view_direction = glm::normalize(spect->ws_view_direction + diff * app::g_delta_time * 3.0f);
        }
        else {
            rotating = 0;
        }
    }
    else if (glm::abs(dmouse_x) > 0.0f || glm::abs(dmouse_y) > 0.0f) {
        vector2_t delta = glm::normalize(vector2_t(dmouse_x, dmouse_y));

        static constexpr float SENSITIVITY = 1.0f;

        vector3_t res = spect->ws_view_direction;

        float x_angle = glm::radians(-delta.x) * SENSITIVITY;// *elapsed;
        float y_angle = glm::radians(-delta.y) * SENSITIVITY;// *elapsed;

        vector3_t up = vector3_t(0.0f, 1.0f, 0.0f);
                
        res = matrix3_t(glm::rotate(x_angle, up)) * res;
        vector3_t rotate_y = glm::cross(res, up);
        res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

        dest = glm::normalize(res);
        rotating = 1;
    }
}

void main_scene_t::tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    handle_input(state);

    nw_tick(state);

    // Submit the mesh
    premade_scene_gpu_sync_and_render(cmdbufs->render_cmdbuf, &structure_);

    vk::render_environment(cmdbufs->render_cmdbuf);

    // Update UI
    // Submits quads to a list that will get sent to the GPU
    ux::tick(state);
    // (from renderer module) - submits the quads to the GPU
    ui::render_submitted_ui(
        cmdbufs->transfer_cmdbuf,
        cmdbufs->ui_cmdbuf);

    ux::scene_info_t *scene_info = ux::get_scene_info();

    vk::eye_3d_info_t *eye_info = &scene_info->eye;
    memset(eye_info, 0, sizeof(vk::eye_3d_info_t));
    vkph::player_t *player = wd_get_spectator();

    eye_info->position = player->ws_position;
    eye_info->direction = player->ws_view_direction;
    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = delta_time();

    vk::lighting_info_t *light_info = &scene_info->lighting;
    memset(light_info, 0, sizeof(vk::lighting_info_t));
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

void main_scene_t::handle_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_ENTER_MAIN_MENU_SCENE: {
        ux::push_panel(ux::SI_MAIN_MENU);
    } break;

    case vkph::ET_REQUEST_TO_JOIN_SERVER: {
        vkph::submit_event(vkph::ET_ENTER_GAME_PLAY_SCENE, NULL);
        ux::bind_scene(ST_PLAY, state);
    } break;

    case vkph::ET_REQUEST_USER_INFORMATION: {
        ux::clear_panels();
        ux::push_panel(ux::SI_SIGN_UP);
    } break;

    case vkph::ET_ENTER_MAP_CREATOR_SCENE: {
        vkph::submit_event(vkph::ET_BEGIN_MAP_EDITING, event->data);
        ux::bind_scene(ST_MAP_CREATOR, state);
    } break;

    default: {
    } break;
    }
}

}
