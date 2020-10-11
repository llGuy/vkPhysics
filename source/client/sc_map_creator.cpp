#include <client/cl_view.hpp>
#include <client/ui_popup.hpp>
#include <common/chunk.hpp>
#include <common/event.hpp>
#include <common/map.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>
#include "ui_core.hpp"
#include "cl_main.hpp"
#include "fx_post.hpp"
#include "wd_core.hpp"
#include "sc_scene.hpp"
#include "wd_predict.hpp"
#include "wd_spectate.hpp"
#include "dr_draw_scene.hpp"
#include "sc_map_creator.hpp"

#include <common/player.hpp>
#include <cstddef>
#include <cstdlib>

enum submode_t {
    S_IN_GAME,
    S_PAUSE,
    S_INVALID
};

static submode_t submode;
static map_t *map;
static bool need_to_save;
static bool started_command;

#define EDIT_BUFFER_MAX_CHAR_COUNT 20

static uint32_t edit_char_count;
static char edit_buffer[EDIT_BUFFER_MAX_CHAR_COUNT] = {};

void sc_map_creator_init(listener_t listener, event_submissions_t *events) {
    subscribe_to_event(ET_PRESSED_ESCAPE, listener, events);
    subscribe_to_event(ET_BEGIN_MAP_EDITING, listener, events);
    subscribe_to_event(ET_CREATE_NEW_MAP, listener, events);
    subscribe_to_event(ET_DONT_CREATE_NEW_MAP, listener, events);

    edit_char_count = 0;
    started_command = 0;
}

void sc_bind_map_creator() {
    fx_disable_blur();
    fx_enable_ssao();

    // Set local player to the spectator
    wd_set_local_player(-1);
}

// All commands end with an enter, and start with a keybinding
// CONTROL+A -> add
// CONTROL+D -> destroy

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

enum edit_command_type_t : char { ECT_ADD = 'A', ECT_DESTROY = 'D', ECT_INVALID = 0 };
enum edit_shape_type_t : char {EST_SPHERE = 's', EST_HOLLOW_SPHERE = 'h', EST_PLANE = 'p', EST_MATH = 'm', EST_INVALID = 0 };

static void s_parse_and_generate_sphere(
    generation_type_t type,
    const char *str,
    uint32_t str_len) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    player_t *spectator = wd_get_spectator();

    generate_sphere(spectator->ws_position, sphere_radius, 140, type);
}

static void s_parse_and_generate_hollow_sphere(
    generation_type_t type,
    const char *str,
    uint32_t str_len) {
    // Parse for the sphere dimension
    char *end_pointer = const_cast<char *>(str + str_len);
    uint32_t sphere_radius = strtol(str + 2, &end_pointer, 10);

    player_t *spectator = wd_get_spectator();

    generate_hollow_sphere(spectator->ws_position, sphere_radius, 250, type);
}

static void s_parse_and_generate_plane(
    generation_type_t type,
    const char *str,
    uint32_t str_len) {
    
}

static void s_parse_and_generate_math_equation(
    generation_type_t type,
    const char *str,
    uint32_t str_len) {
    
}

static void s_parse_and_execute_command(
    const char *str, uint32_t str_len) {
    generation_type_t edit_type = GT_INVALID;
    edit_shape_type_t shape_type = EST_INVALID;

    // Do this switch statement just for error handling
    switch (str[0]) {
    case ECT_ADD: {
        // Executing an ADD command
        edit_type = GT_ADDITIVE;
    } break;

    case ECT_DESTROY: {
        // Executing a DESTROY command
        edit_type = GT_DESTRUCTIVE;
    } break;

    default: {
        LOG_ERRORV("Invalid command type %c\n", str[0]);
    } break;
    }

    switch (str[1]) {
    case EST_SPHERE: {
        LOG_INFO("Generating sphere\n");
        s_parse_and_generate_sphere(edit_type, str, str_len);
    } break;

    case EST_HOLLOW_SPHERE: {
        LOG_INFO("Generating hollow sphere\n");
        s_parse_and_generate_hollow_sphere(edit_type, str, str_len);
    } break;

    case EST_PLANE: {
        
    } break;

    case EST_MATH: {
        
    } break;
    }
}

static void s_editor_input(
    float dt) {
}

static void s_handle_input(event_submissions_t *events) {
    switch (submode) {

    case S_IN_GAME: {
        raw_input_t *raw_input = get_raw_input();

        if (started_command) {
            if (raw_input->buttons[BT_ENTER].instant) {
                LOG_INFO("Executing edit command...\n");

                // Parse and execute the command
                started_command = 0;
                s_parse_and_execute_command(edit_buffer, edit_char_count);
                edit_char_count = 0;
                memset(edit_buffer, 0, sizeof(char) * EDIT_BUFFER_MAX_CHAR_COUNT);
            }
            else {
                for (uint32_t i = 0; i < raw_input->char_count; ++i) {
                    if (raw_input->char_stack[i]) {
                        if (!s_push_edit_character(raw_input->char_stack[i])) {
                            started_command = 0;
                            edit_char_count = 0;
                            LOG_INFO("Exceeded edit characters limit\n");

                            break;
                        }
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
                        s_push_edit_character('A');
                        LOG_INFO("Starting additive edit command\n");
                    } break;
                    case '-': {
                        started_command = 1;
                        s_push_edit_character('D');
                        LOG_INFO("Starting destructive edit command\n");
                    } break;
                    }

                    raw_input->char_stack[i] = 0;
                }
            }

            wd_game_input(cl_delta_time());
        }
    } break;

    case S_PAUSE: {
        ui_handle_input(events);
    } break;

    default: {
    } break;

    }
}

void sc_map_creator_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events) {
    s_handle_input(events);

    // The world always gets ticked - when menus get displayed, the world has to keep being simulated
    wd_execute_player_actions(wd_get_spectator(), events);
    wd_tick(events);

    dr_draw_game(render, transfer);

    ui_tick(events);
    render_submitted_ui(transfer, ui);

    eye_3d_info_t *eye_info = sc_get_eye_info();
    player_t *player = NULL;

    player = wd_get_spectator();

    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();
    eye_info->direction = player->ws_view_direction;
    eye_info->position = player->ws_position;

    lighting_info_t *light_info = sc_get_lighting_info();
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

static void s_exit_map_editor(event_submissions_t *events) {
    save_map(map);

    unload_map(map);
    wd_clear_world();

    ui_clear_panels();
    submit_event(ET_ENTER_MAIN_MENU_SCENE, NULL, events);

    // Bind to main menu, and the main menu will then have to handle the ET_ENTER_MAIN_MENU event
    sc_bind(ST_MAIN_MENU);
}

void sc_handle_map_creator_event(void *object, event_t *event, event_submissions_t *events) {
    switch (event->type) {
    case ET_BEGIN_MAP_EDITING: {
        ui_clear_panels();

        submode = S_IN_GAME;
        cl_change_view_type(GVT_IN_GAME);

        event_enter_map_creator_t *event_data = (event_enter_map_creator_t *)event->data;

        map = load_map(event_data->map_path);
        map->name = event_data->map_name;
        
        if (map->is_new) {
            // Create popup
            ui_popup_t *popup = ui_add_popup(2);
            ui_push_popup_section_text(popup, "Create new?");

            const char *texts[] = { "Yes", "No" };
            void (* procs[2])(ui_popup_t *, event_submissions_t *) = {
                [] (ui_popup_t *, event_submissions_t *events) { submit_event(ET_CREATE_NEW_MAP, NULL, events); },
                [] (ui_popup_t *, event_submissions_t *events) { submit_event(ET_DONT_CREATE_NEW_MAP, NULL, events); }
            };

            ui_push_popup_section_button_double(popup, texts, procs);

            ui_prepare_popup_for_render(popup);

            cl_change_view_type(GVT_MENU);
            submode = S_PAUSE;
        }
        else {
            // TODO: Load map contents from file
            need_to_save = 1;
        }

        FL_FREE(event->data);
    } break;

    case ET_CREATE_NEW_MAP: {
        // Add map to map names
        add_map_name(map->name, map->path);
        ui_pop_panel();

        submode = S_IN_GAME;
        cl_change_view_type(GVT_IN_GAME);

        need_to_save = 1;
    } break;

    case ET_EXIT_SCENE: {
        s_exit_map_editor(events);
        need_to_save = 0;
    } break;

    case ET_DONT_CREATE_NEW_MAP: {
        // Exit map creator
        event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
        effect_data->dest_value = 0.0f;
        effect_data->duration = 2.5f;
        effect_data->fade_back = 1;
        effect_data->trigger_count = 1;
        effect_data->triggers[0].trigger_type = ET_EXIT_SCENE;
        effect_data->triggers[0].next_event_data = NULL;
        submit_event(ET_BEGIN_FADE, effect_data, events);
    } break;

    case ET_PRESSED_ESCAPE: {
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

    default: {
    } break;
    }
}
