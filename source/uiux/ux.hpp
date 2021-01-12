#pragma once

#include <vkph_state.hpp>

#include "ui_font.hpp"

namespace ux {

enum stack_item_t {
    SI_MAIN_MENU,
    SI_HUD,
    SI_GAME_MENU,
    SI_SIGN_UP,
    SI_POPUP,
    SI_INVALID,
};

void init(vkph::state_t *state);
void handle_input(vkph::state_t *state);
void tick(vkph::state_t *state);

void push_panel(stack_item_t item);
void pop_panel();
void clear_panels();

enum ui_texture_t {
    UT_PLAY_ICON,
    UT_SETTINGS_ICON,
    UT_BUILD_ICON,
    UT_QUIT_ICON,
    UT_SPAWN_ICON,
    UT_CROSSHAIRS,
    UT_COLOR_TABLE,
    UT_TEAM_SELECT,
    UT_DAMAGE_INDICATOR,
    UT_INVALID_TEXTURE
};

ui::font_t *get_game_font();
VkDescriptorSet get_texture(ui_texture_t type);

}
