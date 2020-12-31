#include "cl_frame.hpp"
#include "cl_view.hpp"
#include <ux_popup.hpp>
#include <vkph_chunk.hpp>
#include <vkph_map.hpp>
#include <app.hpp>
#include <vk.hpp>
#include <ux_hud.hpp>
#include <ux.hpp>
#include "cl_main.hpp"
#include "ux_scene.hpp"
#include "vkph_terraform.hpp"
#include "wd_core.hpp"
#include "wd_predict.hpp"
#include "wd_spectate.hpp"
#include <ui_submit.hpp>
#include "cl_render.hpp"
#include "cl_scene.hpp"

#include <vkph_player.hpp>
#include <vkph_event_data.hpp>
#include <cstddef>
#include <cstdlib>

namespace cl {

void map_creator_scene_t::init() {
    edit_char_count_ = 0;
    started_command_ = 0;
    display_text_in_minibuffer_ = 0;
}

void map_creator_scene_t::subscribe_to_events(vkph::listener_t listener) {
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, listener);
    vkph::subscribe_to_event(vkph::ET_BEGIN_MAP_EDITING, listener);
    vkph::subscribe_to_event(vkph::ET_CREATE_NEW_MAP, listener);
    vkph::subscribe_to_event(vkph::ET_DONT_CREATE_NEW_MAP, listener);
    vkph::subscribe_to_event(vkph::ET_MAP_EDITOR_CHOSE_COLOR, listener);
}

void map_creator_scene_t::prepare_for_binding(vkph::state_t *state) {
    get_frame_info()->blurred = 0;
    get_frame_info()->ssao = 1;

    // Set local player to the spectator
    wd_set_local_player(-1, state);
    wd_get_spectator()->current_camera_up = wd_get_spectator()->ws_up_vector = glm::normalize(vector3_t(1.0f, 1.0f, 1.0f));

    state->flags.track_history = 0;
}

void map_creator_scene_t::prepare_for_unbinding(vkph::state_t *state) {
    state->save_map();

    wd_clear_world(state);

    ux::clear_panels();
    vkph::submit_event(vkph::ET_ENTER_MAIN_MENU_SCENE, NULL);

    ux::end_minibuffer();
    ux::end_color_table();
    need_to_save_ = 0;
}

// All commands end with an enter, and start with a keybinding
// + -> add
// - -> destroy

// Then, there is a parameter
// s (sphere), h (hollow sphere), p (plane), m (math equation)

// Then, there is yet another parameter (often to do with dimensions)
// number; another numbers; etc...

bool map_creator_scene_t::push_edit_character(char c) {
    if (edit_char_count_ == EDIT_BUFFER_MAX_CHAR_COUNT_) {
        return 0;
    }
    else {
        edit_buffer_[edit_char_count_++] = c;
        return 1;
    }
}

enum edit_command_type_t : char { ECT_ADD = '+', ECT_DESTROY = '-', ECT_INVALID = 0 };
enum edit_shape_type_t : char { EST_SPHERE = 's', EST_HOLLOW_SPHERE = 'h', EST_PLANE = 'p', EST_MATH = 'm', EST_INVALID = 0 };

void map_creator_scene_t::parse_and_generate_sphere(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    vkph::player_t *spectator = wd_get_spectator();

    vkph::sphere_create_info_t info = {};
    info.color = current_color_;
    info.max_value = 140;
    info.type = type;
    info.ws_center = spectator->ws_position;
    info.ws_radius = sphere_radius;
    state->generate_sphere(&info);
}

void map_creator_scene_t::parse_and_generate_hollow_sphere(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    vkph::player_t *spectator = wd_get_spectator();

    vkph::sphere_create_info_t info = {};
    info.color = current_color_;
    info.max_value = 250;
    info.type = type;
    info.ws_center = spectator->ws_position;
    info.ws_radius = sphere_radius;
    state->generate_hollow_sphere(&info);
}

void map_creator_scene_t::parse_and_execute_command(
    const char *str, uint32_t str_len,
    vkph::state_t *state) {
    vkph::generation_type_t edit_type = vkph::GT_INVALID;
    edit_shape_type_t shape_type = EST_INVALID;

    // Do this switch statement just for error handling
    switch (str[0]) {
    case ECT_ADD: {
        // Executing an ADD command
        edit_type = vkph::GT_ADDITIVE;
    } break;

    case ECT_DESTROY: {
        // Executing a DESTROY command
        edit_type = vkph::GT_DESTRUCTIVE;
    } break;

    default: {
        LOG_ERRORV("Invalid command type %c\n", str[0]);
    } break;
    }

    switch (str[1]) {
    case EST_SPHERE: {
        LOG_INFO("Generating sphere\n");
        parse_and_generate_sphere(edit_type, str, str_len, state);
    } break;

    case EST_HOLLOW_SPHERE: {
        LOG_INFO("Generating hollow sphere\n");
        parse_and_generate_hollow_sphere(edit_type, str, str_len, state);
    } break;
    }
}

static void s_editor_input(
    float dt) {
    // ...
}

void map_creator_scene_t::handle_input(vkph::state_t *state) {
    static bool display_color_table = 0;

    switch (submode_) {

    case S_IN_GAME: {
        const app::raw_input_t *raw_input = app::get_raw_input();

        if (started_command_) {
            if (raw_input->buttons[app::BT_ENTER].instant) {
                LOG_INFO("Executing edit command...\n");

                // Parse and execute the command
                started_command_ = 0;
                parse_and_execute_command(edit_buffer_, edit_char_count_, state);
                edit_char_count_ = 0;
                memset(edit_buffer_, 0, sizeof(char) * EDIT_BUFFER_MAX_CHAR_COUNT_);
                ux::minibuffer_update_text("");

                display_text_in_minibuffer_ = 0;
            }
            else {
                for (uint32_t i = 0; i < raw_input->char_count; ++i) {
                    if (raw_input->char_stack[i]) {
                        if (!push_edit_character(raw_input->char_stack[i])) {
                            started_command_ = 0;
                            edit_char_count_ = 0;
                            LOG_INFO("Exceeded edit characters limit\n");

                            display_text_in_minibuffer_ = 0;

                            break;
                        }

                        ux::minibuffer_update_text(edit_buffer_);
                    }
                }
            }
        }
        else {
            for (uint32_t i = 0; i < raw_input->char_count; ++i) {
                if (raw_input->char_stack[i]) {
                    switch(raw_input->char_stack[i]) {
                    case '+': {
                        started_command_ = 1;
                        push_edit_character('+');
                        display_text_in_minibuffer_ = 1;
                        ux::minibuffer_update_text(edit_buffer_);
                        LOG_INFO("Starting additive edit command\n");
                    } break;
                    case '-': {
                        started_command_ = 1;
                        push_edit_character('-');
                        display_text_in_minibuffer_ = 1;
                        ux::minibuffer_update_text(edit_buffer_);
                        LOG_INFO("Starting destructive edit command\n");
                    } break;
                    case 'c': {
                        display_color_table = !display_color_table;

                        if (display_color_table) {
                            ux::begin_color_table();
                            change_view_type(GVT_MENU);
                        }
                        else {
                            ux::end_color_table();
                            change_view_type(GVT_IN_GAME);
                        }
                    } break;
                    }

                    app::nullify_char_at(i);
                }
            }

            if (!display_color_table) {
                wd_game_input(delta_time(), state);
            }
            else {
                ux::handle_input(state);
            }
        }
    } break;

    case S_PAUSE: {
        ux::handle_input(state);
    } break;

    default: {
    } break;

    }
}

void map_creator_scene_t::tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    handle_input(state);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_execute_player_actions(wd_get_spectator(), state);
    wd_tick(state);

    ux::scene_info_t *scene_info = ux::get_scene_info();
    vk::eye_3d_info_t *eye_info = &scene_info->eye;
    vkph::player_t *player = wd_get_spectator();

    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = delta_time();
    eye_info->direction = player->ws_view_direction;
    eye_info->position = player->ws_position;

    draw_game(cmdbufs, state);

    ux::tick(state);
    ui::render_submitted_ui(cmdbufs->transfer_cmdbuf, cmdbufs->ui_cmdbuf);

    vk::lighting_info_t *light_info = &scene_info->lighting;
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

void map_creator_scene_t::handle_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_BEGIN_MAP_EDITING: {
        ux::clear_panels();

        submode_ = S_IN_GAME;
        change_view_type(GVT_IN_GAME);

        auto *event_data = (vkph::event_enter_map_creator_t *)event->data;

        map_ = state->load_map(event_data->map_path);
        map_->name = event_data->map_name;
        
        if (map_->is_new) {
            // Create popup
            ux::popup_t *popup = ux::add_popup(2);
            ux::push_popup_section_text(popup, "Create new?");

            const char *texts[] = { "Yes", "No" };
            void (* procs[2])(ux::popup_t *) = {
                [] (ux::popup_t *) { vkph::submit_event(vkph::ET_CREATE_NEW_MAP, NULL); },
                [] (ux::popup_t *) { vkph::submit_event(vkph::ET_DONT_CREATE_NEW_MAP, NULL); }
            };

            ux::push_popup_section_button_double(popup, texts, procs);

            ux::prepare_popup_for_render(popup);

            change_view_type(GVT_MENU);
            submode_ = S_PAUSE;
        }
        else {
            // TODO: Load map contents from file
            need_to_save_ = 1;
            ux::begin_minibuffer();
            ux::push_panel(ux::SI_HUD);
        }

        FL_FREE(event->data);
    } break;

    case vkph::ET_CREATE_NEW_MAP: {
        // Add map to map names
        state->add_map_name(map_->name, map_->path);
        ux::pop_panel();
        ux::push_panel(ux::SI_HUD);

        submode_ = S_IN_GAME;
        change_view_type(GVT_IN_GAME);

        need_to_save_ = 1;

        ux::begin_minibuffer();
    } break;

    case vkph::ET_EXIT_SCENE: {
        ux::bind_scene(ST_MAIN, state);
    } break;

    case vkph::ET_DONT_CREATE_NEW_MAP: {
        // Exit map creator
        auto *effect_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
        effect_data->dest_value = 0.0f;
        effect_data->duration = 2.5f;
        effect_data->fade_back = 1;
        effect_data->trigger_count = 1;
        effect_data->triggers[0].trigger_type = vkph::ET_EXIT_SCENE;
        effect_data->triggers[0].next_event_data = NULL;
        vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
    } break;

    case vkph::ET_PRESSED_ESCAPE: {
        if (submode_ == S_IN_GAME) {
            ux::push_panel(ux::SI_GAME_MENU);
            change_view_type(GVT_MENU);
            submode_ = S_PAUSE;
        }
        else {
            ux::pop_panel();
            change_view_type(GVT_IN_GAME);
            submode_ = S_IN_GAME;
        }
    } break;

    case vkph::ET_MAP_EDITOR_CHOSE_COLOR: {
        auto *data = (vkph::event_map_editor_chose_color_t *)event->data;
        current_color_ = vkph::b8v_color_to_b8(data->r, data->g, data->b);

        vector3_t v3_color = vkph::b8_color_to_v3(current_color_);

        LOG_INFOV("%f %f %f\n", v3_color.r, v3_color.g, v3_color.b);

        wd_get_spectator()->terraform_package.color = current_color_;

        FL_FREE(data);
    } break;

    default: {
    } break;
    }
}

}
