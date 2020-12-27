#include <common/player.hpp>
#include "cl_view.hpp"
#include "common/chunk.hpp"
#include "nw_client.hpp"
#include "ui_core.hpp"
#include "wd_core.hpp"
#include "sc_play.hpp"
#include "sc_scene.hpp"
#include "fx_post.hpp"
#include "cl_main.hpp"
#include "wd_predict.hpp"
#include "ui_hud.hpp"
#include "wd_spectate.hpp"
#include <common/game.hpp>
#include <common/event.hpp>
#include "dr_draw_scene.hpp"
#include <vk.hpp>
#include <ui.hpp>

enum submode_t {
    S_MENU,
    S_IN_GAME,
    S_PAUSE,
    S_INVALID
};

static submode_t submode;
static bool is_first_person;

void sc_play_init(listener_t listener, event_submissions_t *events) {
    // Nothing to do here
    subscribe_to_event(ET_ENTER_GAME_PLAY_SCENE, listener, events);
    subscribe_to_event(ET_EXIT_SCENE, listener, events);
    subscribe_to_event(ET_SPAWN, listener, events);
    subscribe_to_event(ET_LOCAL_PLAYER_DIED, listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, listener, events);
}

void sc_bind_play() {
    player_t *spect = wd_get_spectator();
    spect->ws_position = vector3_t(-2500.0f, -1000.0f, 5000.0f);
    spect->ws_view_direction = glm::normalize(-spect->ws_position);
    spect->ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);

    fx_disable_blur();
    fx_enable_ssao();

    state->flags.track_history = 1;
}

static void s_handle_input(event_submissions_t *events) {
    switch (submode) {

    case S_MENU: {
        ui_handle_input(events);
    } break;

    case S_IN_GAME: {
        wd_game_input(cl_delta_time());
    } break;

    case S_PAUSE: {
        ui_handle_input(events);
    } break;

    default: {
    } break;

    }
}

static void s_calculate_pos_and_dir(player_t *player, vector3_t *position, vector3_t *direction) {
    // For first person camera
    float camera_distance = player->camera_distance.current;
    bool render_player = 1;

    if (player->flags.interaction_mode == PIM_STANDING) {
        if (player->switching_shapes) {
            float progress = player->shape_switching_time / SHAPE_SWITCH_ANIMATION_TIME;
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
    else if (player->flags.interaction_mode == PIM_BALL) {
        if (player->switching_shapes) {
            float progress = player->shape_switching_time / SHAPE_SWITCH_ANIMATION_TIME;
            camera_distance = progress * camera_distance;
        }
    }

    is_first_person = !render_player;

    *position = player->ws_position - player->ws_view_direction * camera_distance * PLAYER_SCALE;
    *position += player->current_camera_up * PLAYER_SCALE * 2.0f;
    *direction = player->ws_view_direction;

    if (player->flags.interaction_mode == PIM_STANDING && player->flags.moving) {
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

void sc_play_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer shadow,
    event_submissions_t *events) {
    state->timestep_begin(cl_delta_time());

    s_handle_input(events);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_tick(events);
    nw_tick(events);

    vk::eye_3d_info_t *eye_info = sc_get_eye_info();
    player_t *player = NULL;
    int32_t local_id = wd_get_local_player();

    switch (submode) {
    case S_IN_GAME: case S_PAUSE: {
        if (local_id == -1)
            player = wd_get_spectator();
        else
            player = state->get_player(local_id);

        if (player)
            s_calculate_pos_and_dir(player, &eye_info->position, &eye_info->direction);
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
    dr_draw_game(render, transfer, shadow);



    ui_tick(events);
    ui::render_submitted_ui(transfer, ui);

    vk::lighting_info_t *light_info = sc_get_lighting_info();
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;

    // Increments the current tick number
    state->timestep_end();
}

void sc_handle_play_event(void *object, event_t *event, event_submissions_t *events) {
    switch (event->type) {
    case ET_ENTER_GAME_PLAY_SCENE: {
        // Enter game menu
        ui_push_panel(USI_GAME_MENU);
        submode = S_MENU;

        cl_change_view_type(GVT_MENU);
    } break;

    case ET_EXIT_SCENE: {
        ui_clear_panels();
        ui_end_gameplay_display();

        submit_event(ET_ENTER_MAIN_MENU_SCENE, NULL, events);
        submit_event(ET_LEAVE_SERVER, NULL, events);

        sc_bind(ST_MAIN_MENU);
    } break;

    case ET_SPAWN: {
        ui_clear_panels();
        ui_push_panel(USI_HUD);
        ui_begin_gameplay_display();

        cl_change_view_type(GVT_IN_GAME);

        submode = S_IN_GAME;
    } break;

    case ET_PRESSED_ESCAPE: {
        if (submode == S_IN_GAME) {
            ui_push_panel(USI_GAME_MENU);
            cl_change_view_type(GVT_MENU);
            ui_end_gameplay_display();
            submode = S_PAUSE;

            LOG_INFO("Going to pause menu\n");
        }
        else {
            ui_pop_panel();
            cl_change_view_type(GVT_IN_GAME);
            submode = S_IN_GAME;
            ui_begin_gameplay_display();

            LOG_INFO("Going to game play\n");
        }
    } break;

    case ET_LOCAL_PLAYER_DIED: {
        ui_clear_panels();
        ui_push_panel(USI_GAME_MENU);
        ui_end_gameplay_display();

        cl_change_view_type(GVT_MENU);

        submode = S_MENU;
    } break;

    default: {
    } break;
    }
}

bool sc_is_in_first_person() {
    return is_first_person;
}
