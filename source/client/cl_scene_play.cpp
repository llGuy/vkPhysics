#include <vkph_player.hpp>
#include "cl_render.hpp"
#include "cl_view.hpp"
#include "dr_player.hpp"
#include <vkph_chunk.hpp>
#include "nw_client.hpp"
#include <ux.hpp>
#include "ux_scene.hpp"
#include "wd_core.hpp"
#include "fx_post.hpp"
#include "cl_main.hpp"
#include "wd_predict.hpp"
#include <ux_hud.hpp>
#include "wd_spectate.hpp"
#include <vkph_state.hpp>
#include <vkph_event_data.hpp>
#include "dr_draw_scene.hpp"
#include <vk.hpp>
#include <ui_submit.hpp>
#include "cl_scene.hpp"

void play_scene_t::init() {
    /*
      Empty for now.
     */
}

void play_scene_t::subscribe_to_events(vkph::listener_t listener) {
    // Nothing to do here
    vkph::subscribe_to_event(vkph::ET_ENTER_GAME_PLAY_SCENE, listener);
    vkph::subscribe_to_event(vkph::ET_EXIT_SCENE, listener);
    vkph::subscribe_to_event(vkph::ET_SPAWN, listener);
    vkph::subscribe_to_event(vkph::ET_LOCAL_PLAYER_DIED, listener);
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, listener);
}

void play_scene_t::prepare_for_binding(vkph::state_t *state) {
    vkph::player_t *spect = wd_get_spectator();
    spect->ws_position = vector3_t(-2500.0f, -1000.0f, 5000.0f);
    spect->ws_view_direction = glm::normalize(-spect->ws_position);
    spect->ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);

    fx_disable_blur();
    fx_enable_ssao();

    state->flags.track_history = 1;
}

void play_scene_t::prepare_for_unbinding(vkph::state_t *state) {
    ux::clear_panels();
    ux::end_gameplay_display();
}

void play_scene_t::handle_input(vkph::state_t *state) {
    switch (submode_) {

    case S_MENU: {
        ux::handle_input(state);
    } break;

    case S_IN_GAME: {
        wd_game_input(cl_delta_time(), state);
    } break;

    case S_PAUSE: {
        ux::handle_input(state);
    } break;

    default: {
    } break;

    }
}

void play_scene_t::calculate_pos_and_dir(vkph::player_t *player, vector3_t *position, vector3_t *direction) {
    // For first person camera
    float camera_distance = player->camera_distance.current;
    bool render_player = 1;

    if (player->flags.interaction_mode == vkph::PIM_STANDING) {
        if (player->switching_shapes) {
            float progress = player->shape_switching_time / vkph::SHAPE_SWITCH_ANIMATION_TIME;
            camera_distance = (1.0f - progress) * camera_distance;

            if (progress > 0.8f) {
                render_player = 0;
            }
        }
        else {
            camera_distance = 0;
            render_player = 0;
        }
    }
    else if (player->flags.interaction_mode == vkph::PIM_BALL) {
        if (player->switching_shapes) {
            float progress = player->shape_switching_time / vkph::SHAPE_SWITCH_ANIMATION_TIME;
            camera_distance = progress * camera_distance;
        }
    }

    dr_is_first_person() = !render_player;

    *position = player->ws_position - player->ws_view_direction * camera_distance * vkph::PLAYER_SCALE;
    *position += player->current_camera_up * vkph::PLAYER_SCALE * 2.0f;
    *direction = player->ws_view_direction;

    if (player->flags.interaction_mode == vkph::PIM_STANDING && player->flags.moving) {
        // Add view bobbing
        static float angle = 0.0f;
        static float right = 0.0f;
        angle += cl_delta_time() * 10.0f;
        right += cl_delta_time() * 5.0f;
        angle = fmod(angle, glm::radians(360.0f));
        right = fmod(right, glm::radians(360.0f));

        vector3_t view_dest = *position + *direction;

        vector3_t right_vec = glm::normalize(glm::cross(player->ws_view_direction, player->current_camera_up));
        *position += player->current_camera_up * glm::sin(angle) * 0.004f + right_vec * glm::sin(right) * 0.002f;
        *direction = glm::normalize(view_dest - *position);
    }
}

void play_scene_t::tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    state->timestep_begin(cl_delta_time());

    handle_input(state);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_tick(state);
    nw_tick(state);

    ux::scene_info_t *scene_info = ux::get_scene_info();

    vk::eye_3d_info_t *eye_info = &scene_info->eye;
    vkph::player_t *player = NULL;
    int32_t local_id = wd_get_local_player(state);

    switch (submode_) {
    case S_IN_GAME: case S_PAUSE: {
        if (local_id == -1)
            player = wd_get_spectator();
        else
            player = state->get_player(local_id);

        if (player)
            calculate_pos_and_dir(player, &eye_info->position, &eye_info->direction);
    } break;

    case S_MENU: {
        player = wd_get_spectator();
    } break;

    default: {
    } break;
    }

    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();


    // Render what's in the 3D scene
    dr_draw_game(
        cmdbufs->render_command_buffer,
        cmdbufs->transfer_command_buffer,
        cmdbufs->render_shadow_command_buffer,
        state);



    ux::tick(state);
    ui::render_submitted_ui(cmdbufs->transfer_command_buffer, cmdbufs->ui_command_buffer);

    vk::lighting_info_t *light_info = &scene_info->lighting;
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;

    // Increments the current tick number
    state->timestep_end();
}

void play_scene_t::handle_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_ENTER_GAME_PLAY_SCENE: {
        // Enter game menu
        ux::push_panel(ux::SI_GAME_MENU);
        submode_ = S_MENU;

        cl_change_view_type(GVT_MENU);
    } break;

    case vkph::ET_EXIT_SCENE: {
        vkph::submit_event(vkph::ET_ENTER_MAIN_MENU_SCENE, NULL);
        vkph::submit_event(vkph::ET_LEAVE_SERVER, NULL);

        ux::bind_scene(ST_MAIN, state);
    } break;

    case vkph::ET_SPAWN: {
        ux::clear_panels();
        ux::push_panel(ux::SI_HUD);
        ux::begin_gameplay_display();

        cl_change_view_type(GVT_IN_GAME);

        submode_ = S_IN_GAME;
    } break;

    case vkph::ET_PRESSED_ESCAPE: {
        if (submode_ == S_IN_GAME) {
            ux::push_panel(ux::SI_GAME_MENU);
            cl_change_view_type(GVT_MENU);
            ux::end_gameplay_display();
            submode_ = S_PAUSE;

            LOG_INFO("Going to pause menu\n");
        }
        else {
            ux::pop_panel();
            cl_change_view_type(GVT_IN_GAME);
            submode_ = S_IN_GAME;
            ux::begin_gameplay_display();

            LOG_INFO("Going to game play\n");
        }
    } break;

    case vkph::ET_LOCAL_PLAYER_DIED: {
        ux::clear_panels();
        ux::push_panel(ux::SI_GAME_MENU);
        ux::end_gameplay_display();

        cl_change_view_type(GVT_MENU);

        submode_ = S_MENU;
    } break;

    default: {
    } break;
    }
}
