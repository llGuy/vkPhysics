#include "sc_play.hpp"
#include "ui_core.hpp"
#include "dr_rsc.hpp"
#include "sc_main.hpp"
#include "sc_scene.hpp"
#include "fx_post.hpp"
#include "cl_main.hpp"
#include "nw_client.hpp"
#include "wd_spectate.hpp"
#include <vkph_events.hpp>
#include <vkph_player.hpp>
#include <cstddef>
#include <app.hpp>
#include <vk.hpp>
#include <ui.hpp>

static fixed_premade_scene_t scene;

void sc_main_init(vkph::listener_t listener) {
    scene = dr_read_premade_rsc("assets/misc/startup/default.startup");

    vkph::subscribe_to_event(vkph::ET_ENTER_MAIN_MENU_SCENE, listener);
    vkph::subscribe_to_event(vkph::ET_REQUEST_TO_JOIN_SERVER, listener);
    vkph::subscribe_to_event(vkph::ET_REQUEST_USER_INFORMATION, listener);
    vkph::subscribe_to_event(vkph::ET_ENTER_MAP_CREATOR_SCENE, listener);
}

void sc_bind_main() {
    vkph::player_t *spect = wd_get_spectator();

    // Position the player at the right place and view in the right direction
    spect->ws_position = scene.position;
    spect->ws_view_direction = scene.view_direction;
    spect->ws_up_vector = scene.up_vector;

    fx_enable_blur();
    fx_disable_ssao();
}

static void s_handle_input() {
    ui_handle_input();

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

void sc_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, vkph::state_t *state) {
    s_handle_input();

    nw_tick(state);

    // Submit the mesh
    begin_mesh_submission(render, dr_get_shader_rsc(GS_BALL));
    scene.world_render_data.color = vector4_t(0.0f);
    submit_mesh(render, &scene.world_mesh, dr_get_shader_rsc(GS_BALL), {&scene.world_render_data, vk::DEF_MESH_RENDER_DATA_SIZE});

    vk::render_environment(render);

    // Update UI
    // Submits quads to a list that will get sent to the GPU
    ui_tick();
    // (from renderer module) - submits the quads to the GPU
    ui::render_submitted_ui(transfer, ui);

    vk::eye_3d_info_t *eye_info = sc_get_eye_info();
    memset(eye_info, 0, sizeof(vk::eye_3d_info_t));
    vkph::player_t *player = wd_get_spectator();

    eye_info->position = player->ws_position;
    eye_info->direction = player->ws_view_direction;
    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();

    vk::lighting_info_t *light_info = sc_get_lighting_info();
    memset(light_info, 0, sizeof(vk::lighting_info_t));
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

void sc_handle_main_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_ENTER_MAIN_MENU_SCENE: {
        ui_push_panel(USI_MAIN_MENU);
    } break;

    case vkph::ET_REQUEST_TO_JOIN_SERVER: {
        ui_clear_panels();

        vkph::submit_event(vkph::ET_ENTER_GAME_PLAY_SCENE, NULL);

        sc_bind(ST_GAME_PLAY);
    } break;

    case vkph::ET_REQUEST_USER_INFORMATION: {
        ui_clear_panels();
        ui_push_panel(USI_SIGN_UP);
    } break;

    case vkph::ET_ENTER_MAP_CREATOR_SCENE: {
        ui_clear_panels();

        vkph::submit_event(vkph::ET_BEGIN_MAP_EDITING, event->data);

        sc_bind(ST_MAP_CREATOR);
    } break;

    default: {
    } break;
    }
}
