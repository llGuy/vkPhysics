#include <common/player.hpp>
#include "cl_view.hpp"
#include "nw_client.hpp"
#include "ui.hpp"
#include "wd_core.hpp"
#include "gm_play.hpp"
#include "gm_mode.hpp"
#include "fx_post.hpp"
#include "cl_main.hpp"
#include "wd_predict.hpp"
#include "wd_spectate.hpp"
#include <common/game.hpp>
#include <common/event.hpp>
#include "dr_draw_scene.hpp"
#include <renderer/renderer.hpp>

enum submode_t {
    S_MENU,
    S_IN_GAME,
    S_PAUSE,
    S_INVALID
};

static submode_t submode;

void gm_play_init(listener_t listener, event_submissions_t *events) {
    // Nothing to do here
    subscribe_to_event(ET_ENTER_GAME_PLAY, listener, events);
    subscribe_to_event(ET_LEAVE_SERVER, listener, events);
    subscribe_to_event(ET_EXIT_GAME_PLAY, listener, events);
    subscribe_to_event(ET_SPAWN, listener, events);
    subscribe_to_event(ET_LOCAL_PLAYER_DIED, listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, listener, events);
}

void gm_bind_play() {
    fx_disable_blur();
    fx_enable_ssao();
}

static void s_handle_input(event_submissions_t *events) {
    switch (submode) {

    case S_MENU: {
        handle_ui_input(events);
    } break;

    case S_IN_GAME: {
        wd_input(cl_delta_time());
    } break;

    case S_PAUSE: {
        handle_ui_input(events);
    } break;

    default: {
    } break;

    }
}

static void s_calculate_pos_and_dir(player_t *player, vector3_t *position, vector3_t *direction) {
    *position = player->ws_position - player->ws_view_direction * player->camera_distance.current * PLAYER_SCALE.x;
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

void gm_play_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events) {
    timestep_begin(cl_delta_time());

    s_handle_input(events);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_tick(events);

    nw_tick(events);
    dr_draw_game(render, transfer);

    tick_ui(events);
    render_submitted_ui(transfer, ui);


    eye_3d_info_t *eye_info = gm_get_eye_info();
    player_t *player = NULL;
    int32_t local_id = wd_get_local_player();

    switch (submode) {
    case S_IN_GAME: case S_PAUSE: {
        if (local_id == -1)
            player = wd_get_spectator();
        else
            player = get_player(local_id);

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

    lighting_info_t *light_info = gm_get_lighting_info();
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;

    // Increments the current tick number
    timestep_end();
}

void gm_handle_play_event(void *object, event_t *event, event_submissions_t *events) {
    switch (event->type) {
    case ET_ENTER_GAME_PLAY: {
        // Enter game menu
        push_ui_panel(USI_GAME_MENU);
        submode = S_MENU;

        cl_change_view_type(GVT_MENU);
    } break;

        // Leaving the server equates to exiting the game play mode
    case ET_LEAVE_SERVER: case ET_EXIT_GAME_PLAY: {
        clear_ui_panels();

        submit_event(ET_ENTER_MAIN_MENU, NULL, events);

        gm_bind(GMT_MAIN_MENU);
    } break;

    case ET_SPAWN: {
        clear_ui_panels();
        push_ui_panel(USI_HUD);

        cl_change_view_type(GVT_IN_GAME);

        submode = S_IN_GAME;
    } break;

    case ET_PRESSED_ESCAPE: {
        if (submode == S_IN_GAME) {
            push_ui_panel(USI_GAME_MENU);
            cl_change_view_type(GVT_MENU);
            submode = S_PAUSE;
        }
        else {
            pop_ui_panel();
            cl_change_view_type(GVT_IN_GAME);
            submode = S_IN_GAME;
        }
    } break;

    case ET_LOCAL_PLAYER_DIED: {
        clear_ui_panels();
        push_ui_panel(USI_GAME_MENU);

        cl_change_view_type(GVT_MENU);

        submode = S_MENU;
    } break;

    default: {
    } break;
    }
}
