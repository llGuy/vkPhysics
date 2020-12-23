#include "sc_play.hpp"
#include "ui_core.hpp"
#include "dr_rsc.hpp"
#include "sc_main.hpp"
#include "sc_scene.hpp"
#include "fx_post.hpp"
#include "cl_main.hpp"
#include "nw_client.hpp"
#include "wd_spectate.hpp"
#include <common/event.hpp>
#include <common/player.hpp>
#include <cstddef>
#include <app.hpp>
#include <vk.hpp>

static fixed_premade_scene_t scene;

void sc_main_init(listener_t listener, event_submissions_t *events) {
    scene = dr_read_premade_rsc("assets/misc/startup/default.startup");

    subscribe_to_event(ET_ENTER_MAIN_MENU_SCENE, listener, events);
    subscribe_to_event(ET_REQUEST_TO_JOIN_SERVER, listener, events);
    subscribe_to_event(ET_REQUEST_USER_INFORMATION, listener, events);
    subscribe_to_event(ET_ENTER_MAP_CREATOR_SCENE, listener, events);
}

void sc_bind_main() {
    player_t *spect = wd_get_spectator();

    // Position the player at the right place and view in the right direction
    spect->ws_position = scene.position;
    spect->ws_view_direction = scene.view_direction;
    spect->ws_up_vector = scene.up_vector;

    fx_enable_blur();
    fx_disable_ssao();
}

static void s_handle_input(event_submissions_t *events) {
    ui_handle_input(events);

    player_t *spect = wd_get_spectator();
    game_input_t *game_input = get_game_input();

    static bool rotating = 0;
    static vector3_t dest = vector3_t(0.0f);

    float dmouse_x = game_input->mouse_x - game_input->previous_mouse_x;
    float dmouse_y = game_input->mouse_y - game_input->previous_mouse_y;

    if (rotating) {
        vector3_t diff = dest - spect->ws_view_direction;
        if (glm::dot(diff, diff) > 0.00001f) {
            spect->ws_view_direction = glm::normalize(spect->ws_view_direction + diff * surface_delta_time() * 3.0f);
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

void sc_main_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events) {
    s_handle_input(events);

    nw_tick(events);

    // Submit the mesh
    begin_mesh_submission(render, dr_get_shader_rsc(GS_BALL));
    scene.world_render_data.color = vector4_t(0.0f);
    submit_mesh(render, &scene.world_mesh, dr_get_shader_rsc(GS_BALL), {&scene.world_render_data, DEF_MESH_RENDER_DATA_SIZE});

    render_environment(render);

    // Update UI
    // Submits quads to a list that will get sent to the GPU
    ui_tick(events);
    // (from renderer module) - submits the quads to the GPU
    render_submitted_ui(transfer, ui);

    eye_3d_info_t *eye_info = sc_get_eye_info();
    memset(eye_info, 0, sizeof(eye_3d_info_t));
    player_t *player = wd_get_spectator();

    eye_info->position = player->ws_position;
    eye_info->direction = player->ws_view_direction;
    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();

    lighting_info_t *light_info = sc_get_lighting_info();
    memset(light_info, 0, sizeof(lighting_info_t));
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

void sc_handle_main_event(void *object, struct event_t *event, struct event_submissions_t *events) {
    switch (event->type) {
    case ET_ENTER_MAIN_MENU_SCENE: {
        ui_push_panel(USI_MAIN_MENU);
    } break;

    case ET_REQUEST_TO_JOIN_SERVER: {
        ui_clear_panels();

        submit_event(ET_ENTER_GAME_PLAY_SCENE, NULL, events);

        sc_bind(ST_GAME_PLAY);
    } break;

    case ET_REQUEST_USER_INFORMATION: {
        ui_clear_panels();
        ui_push_panel(USI_SIGN_UP);
    } break;

    case ET_ENTER_MAP_CREATOR_SCENE: {
        ui_clear_panels();

        submit_event(ET_BEGIN_MAP_EDITING, event->data, events);

        sc_bind(ST_MAP_CREATOR);
    } break;

    default: {
    } break;
    }
}
