#include <cstddef>
#include "ui_team_select.hpp"
#include "ui_popup.hpp"
#include "ui_core.hpp"
#include "ui_main_menu.hpp"
#include "ui_game_menu.hpp"
#include "ui_hud.hpp"
#include "ui_sign_up.hpp"
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <app.hpp>
#include <vk.hpp>
#include <allocators.hpp>

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static ui_stack_item_t stack_items[10];
static int32_t stack_item_count;

static void s_ui_event_listener(
    void *object,
    vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    // ...
    switch(event->type) {

    case vkph::ET_RECEIVED_AVAILABLE_SERVERS: {
        ui_refresh_main_menu_server_page();
    } break;

    case vkph::ET_RESIZE_SURFACE: {
        // Need to reinitialise UI system
        ui_main_menu_init(state);
        ui_game_menu_init();
        ui_sign_up_menu_init();
        ui_hud_init();

        ui_init_game_menu_for_server(state);
    } break;

    case vkph::ET_META_REQUEST_ERROR: {
        auto *data = (vkph::event_meta_request_error_t *)event->data;

        ui_handle_sign_up_failed((net::request_error_t)data->error_type);

        FL_FREE(data);
    } break;

    case vkph::ET_SIGN_UP_SUCCESS: case vkph::ET_LOGIN_SUCCESS: {
        ui_clear_panels();
        ui_push_panel(USI_MAIN_MENU);
    } break;

    case vkph::ET_ENTER_SERVER: {
        ui_init_game_menu_for_server(state);
    } break;

    case vkph::ET_SPAWN: {
        ui_lock_spawn_button();
    } break;

    case vkph::ET_LOCAL_PLAYER_DIED: {
        ui_unlock_spawn_button();
    } break;

    default: {
    } break;

    }
}

static vkph::listener_t ui_listener;

static ui::font_t *global_font;
static vk::texture_t ui_textures[UT_INVALID_TEXTURE];

ui::font_t *ui_game_font() {
    return global_font;
}

VkDescriptorSet ui_texture(
    ui_texture_t type) {
    return ui_textures[type].descriptor;
}

static void s_ui_textures_init() {
    ui_textures[UT_PLAY_ICON].init("assets/textures/gui/play_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_BUILD_ICON].init("assets/textures/gui/build_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SETTINGS_ICON].init("assets/textures/gui/settings_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_QUIT_ICON].init("assets/textures/gui/quit_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SPAWN_ICON].init("assets/textures/gui/spawn_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_CROSSHAIRS].init("assets/textures/gui/crosshair.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_COLOR_TABLE].init("assets/textures/gui/color_table.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_TEAM_SELECT].init("assets/textures/gui/team.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
}

void ui_init(vkph::state_t *state) {
    stack_item_count = 0;

    ui_listener = vkph::set_listener_callback(&s_ui_event_listener, state);
    vkph::subscribe_to_event(vkph::ET_RECEIVED_AVAILABLE_SERVERS, ui_listener);
    vkph::subscribe_to_event(vkph::ET_RESIZE_SURFACE, ui_listener);
    vkph::subscribe_to_event(vkph::ET_META_REQUEST_ERROR, ui_listener);
    vkph::subscribe_to_event(vkph::ET_SIGN_UP_SUCCESS, ui_listener);
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, ui_listener);
    vkph::subscribe_to_event(vkph::ET_ENTER_SERVER, ui_listener);
    vkph::subscribe_to_event(vkph::ET_SPAWN, ui_listener);
    vkph::subscribe_to_event(vkph::ET_LOCAL_PLAYER_DIED, ui_listener);

    global_font = flmalloc<ui::font_t>();
    global_font->load(
        "assets/font/fixedsys.fnt",
        "assets/font/fixedsys.png");

    // Initialise some textures
    s_ui_textures_init();
    ui_main_menu_init(state);
    ui_hud_init();
    ui_game_menu_init();
    ui_sign_up_menu_init();
    ui_popups_init();
}

void ui_handle_input(vkph::state_t *state) {
    const app::raw_input_t *raw_input = app::get_raw_input();

    if (stack_item_count) {
        ui_stack_item_t last = stack_items[stack_item_count - 1];

        switch (last) {
        case USI_MAIN_MENU: {
            ui_main_menu_input(raw_input);
        } break;

        case USI_GAME_MENU: {
            ui_game_menu_input(raw_input, state);
        } break;

        case USI_SIGN_UP: {
            ui_sign_up_menu_input(raw_input);
        } break;

        case USI_POPUP: {
            ui_popup_input(raw_input);
        } break;

        case USI_HUD: {
            ui_hud_input(raw_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }
}

void ui_tick(vkph::state_t *state) {
    for (uint32_t i = 0; i < stack_item_count; ++i) {
        switch (stack_items[i]) {
        case USI_MAIN_MENU: {
            ui_submit_main_menu();
        } break;

        case USI_HUD: {
            ui_submit_hud(state);
        } break;

        case USI_GAME_MENU: {
            ui_submit_game_menu();
        } break;

        case USI_SIGN_UP: {
            ui_submit_sign_up_menu();
        } break;

        case USI_POPUP: {
            ui_submit_popups();
        } break;

        default: {
        } break;
        }
    }
}

void ui_push_panel(ui_stack_item_t item) {
    stack_items[stack_item_count++] = item;
}

void ui_pop_panel() {
    if (stack_item_count > 0) {
        stack_item_count--;
    }
}

void ui_clear_panels() {
    stack_item_count = 0;
}
