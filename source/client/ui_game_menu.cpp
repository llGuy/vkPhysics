#include <cstddef>
#include "client/ui_team_select.hpp"
#include "nw_client.hpp"
#include "ui_menu_layout.hpp"
#include "ui_game_menu.hpp"
#include "ui_core.hpp"
#include <common/event.hpp>
#include <app.hpp>
#include <vk.hpp>
#include <common/allocators.hpp>

enum button_t {
    B_SPAWN,
    B_SELECT_TEAM,
    B_SETTINGS,
    B_QUIT,
    // ...
    B_INVALID_BUTTON
};

static menu_layout_t game_menu_layout;
static play_button_function_t function;

static void s_menu_layout_disconnect_proc(
    event_submissions_t *events) {
    event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
    effect_data->dest_value = 0.0f;
    effect_data->duration = 2.5f;
    effect_data->fade_back = 1;
    effect_data->trigger_count = 1;
    effect_data->triggers[0].trigger_type = ET_EXIT_SCENE;
    effect_data->triggers[0].next_event_data = NULL;
    submit_event(ET_BEGIN_FADE, effect_data, events);
}

static void s_menu_layout_spawn_proc(
    event_submissions_t *events) {
    event_spawn_t *spawn = FL_MALLOC(event_spawn_t, 1);
    spawn->client_id = nw_get_local_client_index();
    submit_event(ET_SPAWN, spawn, events);
}

void ui_game_menu_init() {
    menu_click_handler_t procs[] = {
        &s_menu_layout_spawn_proc,
        NULL,
        NULL,
        &s_menu_layout_disconnect_proc
    };

    const char *widget_icon_paths[] = { NULL, NULL, NULL, NULL };

    VkDescriptorSet sets[] = {
        ui_texture(UT_SPAWN_ICON),
        ui_texture(UT_TEAM_SELECT),
        ui_texture(UT_SETTINGS_ICON),
        ui_texture(UT_QUIT_ICON),
    };

    game_menu_layout.init(
        procs,
        widget_icon_paths,
        sets,
        B_INVALID_BUTTON);

    game_menu_layout.lock_button(B_SPAWN);

    ui_team_select_init(&game_menu_layout);
}

void ui_init_game_menu_for_server() {
    ui_update_team_roster_layout(&game_menu_layout);
    ui_update_team_roster_display_text(&game_menu_layout);
}

void ui_submit_game_menu() {
    game_menu_layout.submit();

    enum { TEAM_SELECT = 1, SETTINGS = 2 };

    if (game_menu_layout.menu_opened()) {
        switch (game_menu_layout.current_open_menu) {
        case TEAM_SELECT: {
            ui_submit_team_select();
        } break;

        case SETTINGS: {
            // Render the settings page
        } break;
        }
    }
}

void ui_game_menu_input(
    event_submissions_t *events,
    const app::raw_input_t *input) {
    if (game_menu_layout.input(events, input)) {
        enum { TEAM_SELECT = 1, SETTINGS = 2 };

        switch (game_menu_layout.current_open_menu) {
        case TEAM_SELECT: {
            ui_team_select_input(input, events);
        } break;
        }
    }
}

void ui_set_play_button_function(play_button_function_t function) {
    // TODO:
}

void ui_lock_spawn_button() {
    game_menu_layout.lock_button(B_SPAWN);
}

void ui_unlock_spawn_button() {
    game_menu_layout.unlock_button(B_SPAWN);
}
