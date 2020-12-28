#include "ui_core.hpp"
#include "ui_game_menu.hpp"
#include "ui_hover.hpp"
#include "wd_predict.hpp"
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include <vkph_state.hpp>
#include "ui_popup.hpp"
#include <vkph_team.hpp>
#include "ui_menu_layout.hpp"
#include "ui_team_select.hpp"
#include <cstdio>
#include <app.hpp>
#include "ui_hover.hpp"

static ui::box_t head_box;
// "Select team"
static ui::text_t head_text;

struct team_button_t {
    vkph::team_color_t color;

    // Display color
    ui::box_t box;
    // Contain the text
    ui::box_t text_box;
    // Display player count
    ui::text_t player_count_text;

    widget_color_t button_color;
    bool hovered_on;
};

static uint32_t button_count;
static team_button_t *buttons;
static ui::box_t main_box;

void ui_team_select_init(menu_layout_t *layout) {
    head_box.init(ui::RT_LEFT_UP, 10.0f, {0.025f, -0.025f}, {0.3, 0.1f}, &layout->current_menu, 0);
    head_text.init(&head_box, ui_game_font(), ui::text_t::BOTTOM, 0.2f, 0.2f, 15, 1.3f);

    if (head_text.char_count == 0) {
        head_text.draw_string("Select Team", 0xFFFFFFFF);
        head_text.null_terminate();
    }
}

void ui_update_team_roster_layout(
    menu_layout_t *layout,
    const vkph::state_t *state) {
    uint32_t team_count = state->team_count;
    const auto *teams = state->teams;

    button_count = team_count;

    if (buttons)
        FL_FREE(buttons);

    buttons = FL_MALLOC(team_button_t, button_count);

    main_box.init(
        ui::RT_CENTER,
        0.6f * (float)(team_count),
        {0.0f, 0.0f},
        {(float)team_count * 0.3f, 0.7f},
        &layout->current_menu, 0x0);

    float scale = 1.0f / (float)team_count;

    for (uint32_t i = 0; i < team_count; ++i) {
        vkph::team_color_t color = teams[i].make_team_info().color;
        uint32_t rgb_hovered;
        uint32_t rgb_unhovered;

        vkph::team_info_t info = teams[i].make_team_info();

        rgb_hovered = ui::vec4_color_to_ui32b(team_color_to_v4(info.color, 0.9f));
        rgb_unhovered = ui::vec4_color_to_ui32b(team_color_to_v4(info.color, 0.9f) * 0.8f);

        buttons[i].box.init(ui::RT_LEFT_DOWN, 0.75f, {i * scale, 0.0f}, {scale, 1.0f}, &main_box, rgb_unhovered);
        buttons[i].text_box.init(ui::RT_RELATIVE_CENTER, 2.0f, {0.0f, 0.1f}, {0.5f, 0.5f}, &buttons[i].box, 0);
        buttons[i].button_color.init(rgb_unhovered, rgb_hovered, 0, 0xFF);

        buttons[i].color = color;
    }
}

void ui_update_team_roster_display_text(menu_layout_t *layout, const vkph::state_t *state) {
    char buf[10] = {};

    uint32_t team_count = state->team_count;
    const auto *teams = state->teams;

    for (uint32_t i = 0; i < button_count; ++i) {
        vkph::team_info_t info = teams[i].make_team_info();
        sprintf(buf, "%d / %d", info.player_count, info.max_players);

        buttons[i].player_count_text.init(&buttons[i].text_box, ui_game_font(), ui::text_t::BOTTOM, 0.1f, 0.1f, 7, 1.3f);
        buttons[i].player_count_text.draw_string(buf, 0xFFFFFFFF);
    }
}

void ui_submit_team_select() {
    ui::mark_ui_textured_section(ui_game_font()->font_img.descriptor);
    ui::push_color_box(&head_box);
    ui::push_text(&head_text);

    for (uint32_t i = 0; i < button_count; ++i) {
        ui::push_color_box(&buttons[i].box);
        ui::push_text(&buttons[i].player_count_text);
    }
}

// TODO: Make sure there vkph::state_t *state is const
void ui_team_select_input(const app::raw_input_t *raw_input, vkph::state_t *state) {
    int32_t hovered_button = -1;
    for (int32_t i = 0; i < button_count; ++i) {
        bool hovered_over = ui_hover_over_box(
            &buttons[i].box,
            vector2_t(raw_input->cursor_pos_x, raw_input->cursor_pos_y));

        color_pair_t pair = buttons[i].button_color.update(
            MENU_WIDGET_HOVER_COLOR_FADE_SPEED,
            hovered_over);

        if (hovered_over) {
            hovered_button = i;
            buttons[i].hovered_on = 1;
        }
        else {
            buttons[i].hovered_on = 0;
        }

        buttons[i].box.color = pair.current_background;
    }

    if (raw_input->buttons[app::BT_MOUSE_LEFT].instant) {
        int32_t team = hovered_button;

        if (team > -1) {
            // Check if we can join this team (if it is full compared to the other teams)
            vkph::team_color_t color = buttons[team].color;

            if (state->check_team_joinable(color)) {
                // Submit even saying that player just joined team
                LOG_INFO("Local client trying to join team\n");

                auto *d = FL_MALLOC(vkph::event_send_server_team_select_request_t, 1);
                d->color = color;
                vkph::submit_event(vkph::ET_SEND_SERVER_TEAM_SELECT_REQUEST, d);

                vkph::player_t *p = state->get_player(wd_get_local_player());

                state->change_player_team(p, color);

                { // Update UI components
                    ui_init_game_menu_for_server(state);
                    ui_unlock_spawn_button();
                }

                p->terraform_package.color = team_color_to_voxel_color(color);

                if (p->flags.is_alive) {
                    // Trigger a dead event
                    wd_kill_local_player(state);
                }
            }
        }
    }
}
