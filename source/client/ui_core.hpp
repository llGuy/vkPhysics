#pragma once

#include <renderer/renderer.hpp>

enum ui_stack_item_t {
    USI_MAIN_MENU,
    USI_HUD,
    USI_GAME_MENU,
    USI_SIGN_UP,
    USI_POPUP,
    USI_INVALID,
};

void ui_init(struct event_submissions_t *events);
void ui_handle_input(event_submissions_t *events);
void ui_tick(struct event_submissions_t *events);

void ui_push_panel(ui_stack_item_t item);
void ui_pop_panel();
void ui_clear_panels();

enum ui_texture_t {
    UT_PLAY_ICON,
    UT_SETTINGS_ICON,
    UT_BUILD_ICON,
    UT_QUIT_ICON,
    UT_SPAWN_ICON,
    UT_CROSSHAIRS,
    UT_INVALID_TEXTURE
};

struct font_t *ui_game_font();
VkDescriptorSet ui_texture(ui_texture_t type);
