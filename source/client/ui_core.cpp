#include <cstddef>
#include "ui_popup.hpp"
#include "ui_core.hpp"
#include "ui_main_menu.hpp"
#include "ui_game_menu.hpp"
#include "ui_hud.hpp"
#include "ui_sign_up.hpp"
#include <common/meta.hpp>
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>
#include <common/allocators.hpp>

// Whenever a new menu gets opened, it gets added to this stack to which the mouse pointer will be
// having effect (or just be rendered)
static ui_stack_item_t stack_items[10];
static int32_t stack_item_count;

static void s_ui_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    // ...
    switch(event->type) {

    case ET_RECEIVED_AVAILABLE_SERVERS: {
        ui_refresh_main_menu_server_page();
    } break;

    case ET_RESIZE_SURFACE: {
        // Need to reinitialise UI system
        ui_main_menu_init();
        ui_game_menu_init();
        ui_sign_up_menu_init();
        ui_hud_init();
    } break;

    case ET_SPAWN: {
        
    } break;

    case ET_META_REQUEST_ERROR: {
        event_meta_request_error_t *data = (event_meta_request_error_t *)event->data;

        ui_handle_sign_up_failed(data->error_type);

        FL_FREE(data);
    } break;

    case ET_SIGN_UP_SUCCESS: case ET_LOGIN_SUCCESS: {
        ui_clear_panels();
        ui_push_panel(USI_MAIN_MENU);
    } break;

    default: {
    } break;

    }
}

static listener_t ui_listener;

static struct font_t *global_font;
static texture_t ui_textures[UT_INVALID_TEXTURE];

struct font_t *ui_game_font() {
    return global_font;
}

VkDescriptorSet ui_texture(
    ui_texture_t type) {
    return ui_textures[type].descriptor;
}

static void s_ui_textures_init() {
    ui_textures[UT_PLAY_ICON] = create_texture("assets/textures/gui/play_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_BUILD_ICON] = create_texture("assets/textures/gui/build_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SETTINGS_ICON] = create_texture("assets/textures/gui/settings_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_QUIT_ICON] = create_texture("assets/textures/gui/quit_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_SPAWN_ICON] = create_texture("assets/textures/gui/spawn_icon.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
    ui_textures[UT_CROSSHAIRS] = create_texture("assets/textures/gui/crosshair.png", VK_FORMAT_R8G8B8A8_UNORM, NULL, 0, 0, VK_FILTER_LINEAR);
}

void ui_init(
    event_submissions_t *events) {
    stack_item_count = 0;

    ui_listener = set_listener_callback(
        &s_ui_event_listener,
        NULL,
        events);

    subscribe_to_event(ET_RECEIVED_AVAILABLE_SERVERS, ui_listener, events);
    subscribe_to_event(ET_RESIZE_SURFACE, ui_listener, events);
    subscribe_to_event(ET_META_REQUEST_ERROR, ui_listener, events);
    subscribe_to_event(ET_SIGN_UP_SUCCESS, ui_listener, events);
    subscribe_to_event(ET_PRESSED_ESCAPE, ui_listener, events);

    global_font = load_font(
        "assets/font/fixedsys.fnt",
        "assets/font/fixedsys.png");

    // Initialise some textures
    s_ui_textures_init();
    ui_main_menu_init();
    ui_hud_init();
    ui_game_menu_init();
    ui_sign_up_menu_init();
    ui_popups_init();
}

void ui_handle_input(
    event_submissions_t *events) {
    raw_input_t *raw_input = get_raw_input();

    if (stack_item_count) {
        ui_stack_item_t last = stack_items[stack_item_count - 1];

        switch (last) {
        case USI_MAIN_MENU: {
            ui_main_menu_input(events, raw_input);
        } break;

        case USI_GAME_MENU: {
            ui_game_menu_input(events, raw_input);
        } break;

        case USI_SIGN_UP: {
            ui_sign_up_menu_input(events, raw_input);
        } break;

        case USI_POPUP: {
            ui_popup_input(events, raw_input);
        } break;

        default: {
            // Nothing
        } break;
        }
    }
}

void ui_tick(
    event_submissions_t *events) {
    for (uint32_t i = 0; i < stack_item_count; ++i) {
        switch (stack_items[i]) {
        case USI_MAIN_MENU: {
            ui_submit_main_menu();
        } break;

        case USI_HUD: {
            ui_submit_hud();
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
