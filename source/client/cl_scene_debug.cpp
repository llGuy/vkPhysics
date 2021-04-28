#include <ux.hpp>
#include <ui_submit.hpp>
#include <ux_hud.hpp>
#include "cl_view.hpp"
#include "cl_game.hpp"
#include "cl_scene.hpp"
#include "cl_render.hpp"
#include <vkph_team.hpp>
#include "cl_game_predict.hpp"
#include <vkph_state.hpp>

namespace cl {

void debug_scene_t::init() {
    
}

void debug_scene_t::subscribe_to_events(vkph::listener_t listener) {
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, listener);
}

void debug_scene_t::handle_input(vkph::state_t *state) {
    switch (submode_) {

    case S_IN_GAME: {
        game_input(app::g_delta_time, state);
    } break;

    case S_PAUSE: {
        ux::handle_input(state);

        /* 
          We also need to make sure to push the actions of the player (otherwise
          the player doesn't update at all).
        */
        game_input(app::g_delta_time, state, true);
    } break;

    default: {
    } break;

    }
}

void debug_scene_t::tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    state->timestep_begin(app::g_delta_time);

    ux::scene_info_t *scene_info = ux::get_scene_info();
    vk::eye_3d_info_t *eye_info = &scene_info->eye;

    auto *main_player = state->get_player(main_player_id_);
    if (main_player) {
        main_player->flags.is_third_person = 0;

        handle_input(state);
        tick_game(state);

        fill_eye_info(eye_info, main_player);
    }

    draw_game(cmdbufs, state);
    ux::tick(state);
    ui::render_submitted_ui(cmdbufs->transfer_cmdbuf, cmdbufs->ui_cmdbuf);

    vk::lighting_info_t *light_info = &scene_info->lighting;
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;

    state->timestep_end();
}

void debug_scene_t::handle_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_PRESSED_ESCAPE: {
        if (submode_ == S_IN_GAME) {
            change_view_type(GVT_MENU);
            submode_ = S_PAUSE;
        }
        else {
            change_view_type(GVT_IN_GAME);
            submode_ = S_IN_GAME;
        }
    } break;
    }
}

void debug_scene_t::prepare_for_binding(vkph::state_t *state) {
    vkph::player_flags_t flags = {};
    flags.is_local = 1;
    flags.team_color = vkph::team_color_t::RED;
    flags.is_alive = true;
    flags.interaction_mode = vkph::PIM_FLOATING;

    vkph::player_init_info_t info = {};
    info.client_id = 0;
    info.client_name = "Main";
    info.default_speed = 20.0f;
    info.flags = flags.u32;
    info.default_speed = 40.0f;
    info.ws_position = vector3_t(-10.0f);
    info.ws_view_direction = glm::normalize(-info.ws_position);
    info.ws_up_vector = vector3_t(0.0f, 1.0f, 0.0f);

    { // Main player
        auto *main_player = state->add_player();
        main_player->init(&info, state->client_to_local_id_map);
        main_player_id_ = main_player->local_id;
        main_player->flags.is_third_person = false;

        main_player->render = init_player_render();

        state->local_player_id = main_player_id_;
    }

    { // Test player
        auto *test_player = state->add_player();
        info.client_name = "Test0";
        info.ws_position = vector3_t(5.0f, 2.0f, 5.0f);

        flags.is_local = 0;
        flags.interaction_mode = vkph::PIM_STANDING;

        info.flags = flags.u32;
        test_player->init(&info, state->client_to_local_id_map);

        test_player->render = init_player_render();
    }

    vkph::platform_create_info_t platform;
    platform.color = vkph::v3_color_to_b8(vector3_t(1.0f));
    platform.position = vector3_t(0.0f);
    platform.width = 30.0f;
    platform.depth = 30.0f;
    platform.type = vkph::GT_ADDITIVE;
    state->generate_platform(&platform);

    get_frame_info()->blurred = 0;
    get_frame_info()->ssao = 1;

    state->flags.track_history = 0;
    
    change_view_type(GVT_IN_GAME);
}

void debug_scene_t::prepare_for_unbinding(vkph::state_t *state) {
    ux::clear_panels();
    ux::end_gameplay_display();
}

}
