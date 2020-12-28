#include "cl_view.hpp"
#include "ui_popup.hpp"
#include <vkph_chunk.hpp>
#include <vkph_map.hpp>
#include <app.hpp>
#include <vk.hpp>
#include "ui_hud.hpp"
#include "ui_core.hpp"
#include "cl_main.hpp"
#include "fx_post.hpp"
#include "vkph_terraform.hpp"
#include "wd_core.hpp"
#include "sc_scene.hpp"
#include "wd_predict.hpp"
#include "wd_spectate.hpp"
#include "dr_draw_scene.hpp"
#include "sc_map_creator.hpp"

#include <vkph_player.hpp>
#include <vkph_event_data.hpp>
#include <cstddef>
#include <cstdlib>

enum submode_t {
    S_IN_GAME,
    S_PAUSE,
    S_INVALID
};

static submode_t submode;
static vkph::map_t *map;
static bool need_to_save;
static bool started_command;

#define EDIT_BUFFER_MAX_CHAR_COUNT 20

static uint32_t edit_char_count;
static char edit_buffer[EDIT_BUFFER_MAX_CHAR_COUNT] = {};
static vkph::voxel_color_t current_color;
static bool display_text_in_minibuffer;

void sc_map_creator_init(vkph::listener_t listener) {
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, listener);
    vkph::subscribe_to_event(vkph::ET_BEGIN_MAP_EDITING, listener);
    vkph::subscribe_to_event(vkph::ET_CREATE_NEW_MAP, listener);
    vkph::subscribe_to_event(vkph::ET_DONT_CREATE_NEW_MAP, listener);
    vkph::subscribe_to_event(vkph::ET_MAP_EDITOR_CHOSE_COLOR, listener);

    edit_char_count = 0;
    started_command = 0;
    display_text_in_minibuffer = 0;
}

void sc_bind_map_creator(vkph::state_t *state) {
    fx_disable_blur();
    fx_enable_ssao();

    // Set local player to the spectator
    wd_set_local_player(-1);
    wd_get_spectator()->current_camera_up = wd_get_spectator()->ws_up_vector = glm::normalize(vector3_t(1.0f, 1.0f, 1.0f));

    state->flags.track_history = 0;
}

// All commands end with an enter, and start with a keybinding
// + -> add
// - -> destroy

// Then, there is a parameter
// s (sphere), h (hollow sphere), p (plane), m (math equation)

// Then, there is yet another parameter (often to do with dimensions)
// number; another numbers; etc...

static bool s_push_edit_character(char c) {
    if (edit_char_count == EDIT_BUFFER_MAX_CHAR_COUNT) {
        return 0;
    }
    else {
        edit_buffer[edit_char_count++] = c;
        return 1;
    }
}

enum edit_command_type_t : char { ECT_ADD = '+', ECT_DESTROY = '-', ECT_INVALID = 0 };
enum edit_shape_type_t : char { EST_SPHERE = 's', EST_HOLLOW_SPHERE = 'h', EST_PLANE = 'p', EST_MATH = 'm', EST_INVALID = 0 };

static void s_parse_and_generate_sphere(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    vkph::player_t *spectator = wd_get_spectator();

    vkph::sphere_create_info_t info = {};
    info.color = current_color;
    info.max_value = 140;
    info.type = type;
    info.ws_center = spectator->ws_position;
    info.ws_radius = sphere_radius;
    state->generate_sphere(&info);
}

static void s_parse_and_generate_hollow_sphere(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    vkph::player_t *spectator = wd_get_spectator();

    vkph::sphere_create_info_t info = {};
    info.color = current_color;
    info.max_value = 250;
    info.type = type;
    info.ws_center = spectator->ws_position;
    info.ws_radius = sphere_radius;
    state->generate_hollow_sphere(&info);
}

static void s_parse_and_generate_plane(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // ...
}

static void s_parse_and_generate_math_equation(
    vkph::generation_type_t type,
    const char *str,
    uint32_t str_len,
    vkph::state_t *state) {
    // ...
}

static void s_parse_and_execute_command(
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
        s_parse_and_generate_sphere(edit_type, str, str_len, state);
    } break;

    case EST_HOLLOW_SPHERE: {
        LOG_INFO("Generating hollow sphere\n");
        s_parse_and_generate_hollow_sphere(edit_type, str, str_len, state);
    } break;

    case EST_PLANE: {
        
    } break;

    case EST_MATH: {
        
    } break;
    }
}

static void s_editor_input(
    float dt) {
    // ...
}

static void s_handle_input(vkph::state_t *state) {
    static bool display_color_table = 0;

    switch (submode) {

    case S_IN_GAME: {
        const app::raw_input_t *raw_input = app::get_raw_input();

        if (started_command) {
            if (raw_input->buttons[app::BT_ENTER].instant) {
                LOG_INFO("Executing edit command...\n");

                // Parse and execute the command
                started_command = 0;
                s_parse_and_execute_command(edit_buffer, edit_char_count, state);
                edit_char_count = 0;
                memset(edit_buffer, 0, sizeof(char) * EDIT_BUFFER_MAX_CHAR_COUNT);
                ui_minibuffer_update_text("");

                display_text_in_minibuffer = 0;
            }
            else {
                for (uint32_t i = 0; i < raw_input->char_count; ++i) {
                    if (raw_input->char_stack[i]) {
                        if (!s_push_edit_character(raw_input->char_stack[i])) {
                            started_command = 0;
                            edit_char_count = 0;
                            LOG_INFO("Exceeded edit characters limit\n");

                            display_text_in_minibuffer = 0;

                            break;
                        }

                        ui_minibuffer_update_text(edit_buffer);
                    }
                }
            }
        }
        else {
            for (uint32_t i = 0; i < raw_input->char_count; ++i) {
                if (raw_input->char_stack[i]) {
                    switch(raw_input->char_stack[i]) {
                    case '+': {
                        started_command = 1;
                        s_push_edit_character('+');
                        display_text_in_minibuffer = 1;
                        ui_minibuffer_update_text(edit_buffer);
                        LOG_INFO("Starting additive edit command\n");
                    } break;
                    case '-': {
                        started_command = 1;
                        s_push_edit_character('-');
                        display_text_in_minibuffer = 1;
                        ui_minibuffer_update_text(edit_buffer);
                        LOG_INFO("Starting destructive edit command\n");
                    } break;
                    case 'c': {
                        display_color_table = !display_color_table;

                        if (display_color_table) {
                            ui_begin_color_table();
                            cl_change_view_type(GVT_MENU);
                        }
                        else {
                            ui_end_color_table();
                            cl_change_view_type(GVT_IN_GAME);
                        }
                    } break;
                    }

                    app::nullify_char_at(i);
                }
            }

            if (!display_color_table) {
                wd_game_input(cl_delta_time(), state);
            }
            else {
                ui_handle_input(state);
            }
        }
    } break;

    case S_PAUSE: {
        ui_handle_input(state);
    } break;

    default: {
    } break;

    }
}

void sc_map_creator_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer render_shadow,
    vkph::state_t *state) {
    s_handle_input(state);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_execute_player_actions(wd_get_spectator(), state);
    wd_tick(state);

    vk::eye_3d_info_t *eye_info = sc_get_eye_info();
    vkph::player_t *player = wd_get_spectator();

    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();
    eye_info->direction = player->ws_view_direction;
    eye_info->position = player->ws_position;

    dr_draw_game(render, transfer, render_shadow, state);

    ui_tick(state);
    ui::render_submitted_ui(transfer, ui);

    vk::lighting_info_t *light_info = sc_get_lighting_info();
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

static void s_exit_map_editor(vkph::state_t *state) {
    state->save_map();

    wd_clear_world(state);

    ui_clear_panels();
    vkph::submit_event(vkph::ET_ENTER_MAIN_MENU_SCENE, NULL);

    // Bind to main menu, and the main menu will then have to handle the ET_ENTER_MAIN_MENU event
    sc_bind(ST_MAIN_MENU, state);
}

void sc_handle_map_creator_event(void *object, vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    switch (event->type) {
    case vkph::ET_BEGIN_MAP_EDITING: {
        ui_clear_panels();

        submode = S_IN_GAME;
        cl_change_view_type(GVT_IN_GAME);

        auto *event_data = (vkph::event_enter_map_creator_t *)event->data;

        map = state->load_map(event_data->map_path);
        map->name = event_data->map_name;
        
        if (map->is_new) {
            // Create popup
            ui_popup_t *popup = ui_add_popup(2);
            ui_push_popup_section_text(popup, "Create new?");

            const char *texts[] = { "Yes", "No" };
            void (* procs[2])(ui_popup_t *) = {
                [] (ui_popup_t *) { vkph::submit_event(vkph::ET_CREATE_NEW_MAP, NULL); },
                [] (ui_popup_t *) { vkph::submit_event(vkph::ET_DONT_CREATE_NEW_MAP, NULL); }
            };

            ui_push_popup_section_button_double(popup, texts, procs);

            ui_prepare_popup_for_render(popup);

            cl_change_view_type(GVT_MENU);
            submode = S_PAUSE;
        }
        else {
            // TODO: Load map contents from file
            need_to_save = 1;
            ui_begin_minibuffer();
            ui_push_panel(USI_HUD);
        }

        FL_FREE(event->data);
    } break;

    case vkph::ET_CREATE_NEW_MAP: {
        // Add map to map names
        state->add_map_name(map->name, map->path);
        ui_pop_panel();
        ui_push_panel(USI_HUD);

        submode = S_IN_GAME;
        cl_change_view_type(GVT_IN_GAME);

        need_to_save = 1;

        ui_begin_minibuffer();
    } break;

    case vkph::ET_EXIT_SCENE: {
        ui_end_minibuffer();
        ui_end_color_table();
        s_exit_map_editor(state);
        need_to_save = 0;
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
        if (submode == S_IN_GAME) {
            ui_push_panel(USI_GAME_MENU);
            cl_change_view_type(GVT_MENU);
            submode = S_PAUSE;
        }
        else {
            ui_pop_panel();
            cl_change_view_type(GVT_IN_GAME);
            submode = S_IN_GAME;
        }
    } break;

    case vkph::ET_MAP_EDITOR_CHOSE_COLOR: {
        auto *data = (vkph::event_map_editor_chose_color_t *)event->data;
        current_color = vkph::b8v_color_to_b8(data->r, data->g, data->b);

        vector3_t v3_color = vkph::b8_color_to_v3(current_color);

        LOG_INFOV("%f %f %f\n", v3_color.r, v3_color.g, v3_color.b);

        wd_get_spectator()->terraform_package.color = current_color;

        FL_FREE(data);
    } break;

    default: {
    } break;
    }
}
