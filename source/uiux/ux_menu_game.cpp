#include "ux.hpp"
#include "ux_menu_game.hpp"
#include "ux_menu_layout.hpp"
#include "ux_page_team_select.hpp"

#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <allocators.hpp>

namespace ux {

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

static void s_menu_layout_disconnect_proc() {
    auto *effect_data = flmalloc<vkph::event_begin_fade_effect_t>(1);
    effect_data->dest_value = 0.0f;
    effect_data->duration = 2.5f;
    effect_data->fade_back = 1;
    effect_data->trigger_count = 1;
    effect_data->triggers[0].trigger_type = vkph::ET_EXIT_SCENE;
    effect_data->triggers[0].next_event_data = NULL;
    vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
}

static void s_menu_layout_spawn_proc() {
    auto *spawn = flmalloc<vkph::event_spawn_t>(1);
    spawn->client_id = -1;
    vkph::submit_event(vkph::ET_SPAWN, spawn);
}

void init_game_menu() {
    menu_click_handler_t procs[] = {
        &s_menu_layout_spawn_proc,
        NULL,
        NULL,
        &s_menu_layout_disconnect_proc
    };

    const char *widget_icon_paths[] = { NULL, NULL, NULL, NULL };

    VkDescriptorSet sets[] = {
        get_texture(UT_SPAWN_ICON),
        get_texture(UT_TEAM_SELECT),
        get_texture(UT_SETTINGS_ICON),
        get_texture(UT_QUIT_ICON),
    };

    game_menu_layout.init(
        procs,
        widget_icon_paths,
        sets,
        B_INVALID_BUTTON);

    game_menu_layout.lock_button(B_SPAWN);

    init_team_select_page(&game_menu_layout);
}

void init_game_menu_for_server(const vkph::state_t *state) {
    update_team_roster_layout(&game_menu_layout, state);
    update_team_roster_display_text(&game_menu_layout, state);
}

void submit_game_menu() {
    game_menu_layout.submit();

    enum { TEAM_SELECT = 1, SETTINGS = 2 };

    if (game_menu_layout.menu_opened()) {
        switch (game_menu_layout.current_open_menu) {
        case TEAM_SELECT: {
            submit_team_select_page();
        } break;

        case SETTINGS: {
            // Render the settings page
        } break;
        }
    }
}

void game_menu_input(
    const app::raw_input_t *input,
    vkph::state_t *state) {
    if (game_menu_layout.input(input)) {
        enum { TEAM_SELECT = 1, SETTINGS = 2 };

        switch (game_menu_layout.current_open_menu) {
        case TEAM_SELECT: {
            team_select_page_input(input, state);
        } break;
        }
    }
}

void set_play_button_function(play_button_function_t function) {
    // TODO:
}

void lock_spawn_button() {
    game_menu_layout.lock_button(B_SPAWN);
}

void unlock_spawn_button() {
    game_menu_layout.unlock_button(B_SPAWN);
}

}
