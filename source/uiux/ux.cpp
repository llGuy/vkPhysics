#include "ux.hpp"
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>

#include <cstddef>
#include "ux_page_team_select.hpp"
#include "ux_popup.hpp"
#include "ux_menu_main.hpp"
#include "ux_menu_game.hpp"
#include "ux_hud.hpp"
#include "ux_menu_sign_up.hpp"
#include "vkph_event.hpp"
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <app.hpp>
#include <vk.hpp>
#include <allocators.hpp>

namespace ux {

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static stack_item_t stack_items[10];
static int32_t stack_item_count;

static void s_ui_event_listener(
    void *object,
    vkph::event_t *event) {
    auto *state = (vkph::state_t *)object;

    // ...
    switch(event->type) {

    case vkph::ET_RECEIVED_AVAILABLE_SERVERS: {
        refresh_main_menu_server_page();
    } break;

    case vkph::ET_RESIZE_SURFACE: {
        // Need to reinitialise UI system
        init_main_menu(state);
        init_game_menu();
        init_sign_up_menu();
        init_hud();

        init_game_menu_for_server(state);
    } break;

    case vkph::ET_META_REQUEST_ERROR: {
        auto *data = (vkph::event_meta_request_error_t *)event->data;

        handle_sign_up_failed((net::request_error_t)data->error_type);

        flfree(data);
    } break;

    case vkph::ET_SIGN_UP_SUCCESS: case vkph::ET_LOGIN_SUCCESS: {
        clear_panels();
        push_panel(SI_MAIN_MENU);
    } break;

    case vkph::ET_ENTER_SERVER: {
        init_game_menu_for_server(state);
    } break;

    case vkph::ET_SPAWN: {
        lock_spawn_button();
    } break;

    case vkph::ET_LOCAL_PLAYER_DIED: {
        unlock_spawn_button();
    } break;

    case vkph::ET_CLIENT_TOOK_DAMAGE: {
        auto *data = (vkph::event_client_took_damage_t *)event->data;
        add_damage_indicator(data->view_dir, data->up, data->right, data->bullet_src_dir);
        flfree(data);
    } break;

    default: {
    } break;

    }
}

static vkph::listener_t listener;

static ui::font_t *global_font;
static vk::texture_t textures[UT_INVALID_TEXTURE];

ui::font_t *get_game_font() {
    return global_font;
}

VkDescriptorSet get_texture(ui_texture_t type) {
    return textures[type].descriptor;
}

static void s_ui_textures_init() {
    textures[UT_PLAY_ICON].init("assets/textures/gui/play_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_BUILD_ICON].init("assets/textures/gui/build_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_SETTINGS_ICON].init("assets/textures/gui/settings_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_QUIT_ICON].init("assets/textures/gui/quit_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_SPAWN_ICON].init("assets/textures/gui/spawn_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_CROSSHAIRS].init("assets/textures/gui/crosshair.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_COLOR_TABLE].init("assets/textures/gui/color_table.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_TEAM_SELECT].init("assets/textures/gui/team.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    textures[UT_DAMAGE_INDICATOR].init("assets/textures/gui/damage.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
}

void init(vkph::state_t *state) {
    stack_item_count = 0;

    listener = vkph::set_listener_callback(&s_ui_event_listener, state);
    vkph::subscribe_to_event(vkph::ET_RECEIVED_AVAILABLE_SERVERS, listener);
    vkph::subscribe_to_event(vkph::ET_RESIZE_SURFACE, listener);
    vkph::subscribe_to_event(vkph::ET_META_REQUEST_ERROR, listener);
    vkph::subscribe_to_event(vkph::ET_SIGN_UP_SUCCESS, listener);
    vkph::subscribe_to_event(vkph::ET_PRESSED_ESCAPE, listener);
    vkph::subscribe_to_event(vkph::ET_ENTER_SERVER, listener);
    vkph::subscribe_to_event(vkph::ET_SPAWN, listener);
    vkph::subscribe_to_event(vkph::ET_RECEIVED_AVAILABLE_SERVERS, listener);
    vkph::subscribe_to_event(vkph::ET_LOCAL_PLAYER_DIED, listener);
    vkph::subscribe_to_event(vkph::ET_CLIENT_TOOK_DAMAGE, listener);

    global_font = flmalloc<ui::font_t>();
    global_font->load(
        "assets/font/fixedsys.fnt",
        "assets/font/fixedsys.png");

    // Initialise some textures
    s_ui_textures_init();
    init_main_menu(state);
    init_hud();
    init_game_menu();
    init_sign_up_menu();
    init_popups();
}

void handle_input(vkph::state_t *state) {
    const app::raw_input_t *raw_input = app::get_raw_input();

    if (stack_item_count) {
        stack_item_t last = stack_items[stack_item_count - 1];

        switch (last) {
        case SI_MAIN_MENU: {
            main_menu_input(raw_input);
        } break;

        case SI_GAME_MENU: {
            game_menu_input(raw_input, state);
        } break;

        case SI_SIGN_UP: {
            sign_up_menu_input(raw_input);
        } break;

        case SI_POPUP: {
            popup_input(raw_input);
        } break;

        case SI_HUD: {
            hud_input(raw_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }
}

void tick(vkph::state_t *state) {
    for (int32_t i = 0; i < stack_item_count; ++i) {
        switch (stack_items[i]) {
        case SI_MAIN_MENU: {
            submit_main_menu();
        } break;

        case SI_HUD: {
            submit_hud(state);
        } break;

        case SI_GAME_MENU: {
            submit_game_menu();
        } break;

        case SI_SIGN_UP: {
            submit_sign_up_menu();
        } break;

        case SI_POPUP: {
            submit_popups();
        } break;

        default: {
        } break;
        }
    }
}

void push_panel(stack_item_t item) {
    stack_items[stack_item_count++] = item;
}

void pop_panel() {
    if (stack_item_count > 0) {
        stack_item_count--;
    }
}

void clear_panels() {
    stack_item_count = 0;
}

}
