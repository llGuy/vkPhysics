#include "client/cl_view.hpp"
#include "client/u_popup.hpp"
#include "common/event.hpp"
#include "common/map.hpp"
#include "renderer/input.hpp"
#include "renderer/renderer.hpp"
#include "ui.hpp"
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

enum submode_t {
    S_IN_GAME,
    S_PAUSE,
    S_INVALID
};

static submode_t submode;
static map_t *map;
static bool started_command;
static char *command_buffer = NULL;

void sc_map_creator_init(listener_t listener, event_submissions_t *events) {
    subscribe_to_event(ET_PRESSED_ESCAPE, listener, events);
    subscribe_to_event(ET_BEGIN_MAP_EDITING, listener, events);
    subscribe_to_event(ET_CREATE_NEW_MAP, listener, events);
    subscribe_to_event(ET_DONT_CREATE_NEW_MAP, listener, events);

    command_buffer = FL_MALLOC(char, 30);
    memset(command_buffer, 0, sizeof(char) * 30);
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
// s (sphere), h (hollow sphere), p (plane)

// Then, there is yet another parameter (often to do with dimensions)
// number; another numbers; etc...

// EACH ELEMENT OF A COMMAND IS SEPARATED BY A SEMI-COLON


static void s_editor_input(
    float dt) {
    raw_input_t *raw_input = get_raw_input();

    if (raw_input->buttons[BT_ENTER].instant) {
        // End command, parse and execute
    }
    else {
        // Add to the command buffer
    }
}

static void s_handle_input(event_submissions_t *events) {
    switch (submode) {

    case S_IN_GAME: {
        // Handles all that is to do with movement
        wd_game_input(cl_delta_time());
        s_editor_input(cl_delta_time());
    } break;

    case S_PAUSE: {
        handle_ui_input(events);
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

    tick_ui(events);
    render_submitted_ui(transfer, ui);

    eye_3d_info_t *eye_info = sc_get_eye_info();
    player_t *player = NULL;

    player = wd_get_spectator();

    eye_info->up = player->current_camera_up;

    eye_info->fov = player->camera_fov.current;
    eye_info->near = 0.01f;
    eye_info->far = 10000.0f;
    eye_info->dt = cl_delta_time();

    lighting_info_t *light_info = sc_get_lighting_info();
    light_info->ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    light_info->lights_count = 0;
}

static void s_exit_map_editor(event_submissions_t *events) {
    unload_map(map);
    wd_clear_world();

    clear_ui_panels();
    submit_event(ET_ENTER_MAIN_MENU, NULL, events);

    // Bind to main menu, and the main menu will then have to handle the ET_ENTER_MAIN_MENU event
    sc_bind(ST_MAIN_MENU);
}

void sc_handle_map_creator_event(void *object, event_t *event, event_submissions_t *events) {
    switch (event->type) {
    case ET_BEGIN_MAP_EDITING: {
        clear_ui_panels();

        submode = S_IN_GAME;
        cl_change_view_type(GVT_IN_GAME);

        event_enter_map_creator_t *event_data = (event_enter_map_creator_t *)event->data;

        map = load_map(event_data->map_path);
        if (map->is_new) {
            // Create popup
            ui_popup_t *popup = u_add_popup(2);
            u_push_popup_section_text(popup, "Create new?");

            const char *texts[] = { "Yes", "No" };
            void (* procs[2])(ui_popup_t *, event_submissions_t *) = {
                [] (ui_popup_t *, event_submissions_t *events) { submit_event(ET_CREATE_NEW_MAP, NULL, events); },
                [] (ui_popup_t *, event_submissions_t *events) { submit_event(ET_DONT_CREATE_NEW_MAP, NULL, events); }
            };

            u_push_popup_section_button_double(popup, texts, procs);

            u_prepare_popup_for_render(popup);

            cl_change_view_type(GVT_MENU);
            submode = S_PAUSE;
        }
        else {
            // TODO: Load map contents from file
        }

        FL_FREE(event->data);
    } break;

    case ET_CREATE_NEW_MAP: {
        // Add map to map names
        add_map_name(map->name, map->path);
    } break;

    case ET_EXIT_SCENE: {
        s_exit_map_editor(events);
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

    default: {
    } break;
    }
}
