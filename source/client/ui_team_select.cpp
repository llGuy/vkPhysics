#include "client/ui_core.hpp"
#include "client/ui_game_menu.hpp"
#include "client/ui_hover.hpp"
#include "client/wd_predict.hpp"
#include "common/event.hpp"
#include "common/player.hpp"
#include "renderer/renderer.hpp"
#include "ui_popup.hpp"
#include <common/team.hpp>
#include "ui_menu_layout.hpp"
#include "ui_team_select.hpp"
#include <common/game.hpp>
#include <cstdio>
#include <renderer/input.hpp>
#include "ui_hover.hpp"

static ui_box_t head_box;
// "Select team"
static ui_text_t head_text;

struct team_button_t {
    team_color_t color;

    // Display color
    ui_box_t box;
    // Contain the text
    ui_box_t text_box;
    // Display player count
    ui_text_t player_count_text;

    widget_color_t button_color;
    bool hovered_on;
};

static uint32_t button_count;
static team_button_t *buttons;
static ui_box_t main_box;

void ui_team_select_init(
    menu_layout_t *layout) {
    head_box.init(RT_LEFT_UP, 10.0f, {0.025f, -0.025f}, {0.3, 0.1f}, &layout->current_menu, 0);
    head_text.init(&head_box, ui_game_font(), ui_text_t::BOTTOM, 0.2f, 0.2f, 15, 1.3f);

    if (head_text.char_count == 0) {
        head_text.draw_string("Select Team", 0xFFFFFFFF);
        head_text.null_terminate();
    }
}

void ui_update_team_roster_layout(
    menu_layout_t *layout) {
    uint32_t team_count = 0;
    team_t *teams = get_teams(&team_count);

    button_count = team_count;

    if (buttons)
        FL_FREE(buttons);

    buttons = FL_MALLOC(team_button_t, button_count);

    main_box.init(
        RT_CENTER,
        0.6f * (float)(team_count),
        {0.0f, 0.0f},
        {(float)team_count * 0.3f, 0.7f},
        &layout->current_menu, 0x0);

    float scale = 1.0f / (float)team_count;

    for (uint32_t i = 0; i < team_count; ++i) {
        team_color_t color = teams[i].make_team_info().color;
        uint32_t rgb_hovered;
        uint32_t rgb_unhovered;

        team_info_t info = teams[i].make_team_info();

        switch (info.color) {
        case team_color_t::RED: {
            rgb_hovered = 0xEE0000CC;
            rgb_unhovered = 0xFF0000AA;
        } break;

        case team_color_t::BLUE: {
            rgb_hovered = 0x0000EECC;
            rgb_unhovered = 0x0000FFAA;
        } break;

        case team_color_t::PURPLE: {
            rgb_hovered = 0xEE00EECC;
            rgb_unhovered = 0xFF00FFAA;
        } break;
        }

        buttons[i].box.init(RT_LEFT_DOWN, 0.75f, {i * scale, 0.0f}, {scale, 1.0f}, &main_box, rgb_unhovered);
        buttons[i].text_box.init(RT_RELATIVE_CENTER, 2.0f, {0.0f, 0.1f}, {0.5f, 0.5f}, &buttons[i].box, 0);
        buttons[i].button_color.init(rgb_unhovered, rgb_hovered, 0, 0xFF);

        buttons[i].color = color;
    }
}

void ui_update_team_roster_display_text(menu_layout_t *layout) {
    char buf[10] = {};

    uint32_t team_count = 0;
    team_t *teams = get_teams(&team_count);

    for (uint32_t i = 0; i < button_count; ++i) {
        team_info_t info = teams[i].make_team_info();
        sprintf(buf, "%d / %d", info.player_count, info.max_players);

        buttons[i].player_count_text.init(&buttons[i].text_box, ui_game_font(), ui_text_t::BOTTOM, 0.1f, 0.1f, 7, 1.3f);
        buttons[i].player_count_text.draw_string(buf, 0xFFFFFFFF);
    }
}

void ui_submit_team_select() {
    mark_ui_textured_section(ui_game_font()->font_img.descriptor);
    push_colored_ui_box(&head_box);
    push_ui_text(&head_text);

    for (uint32_t i = 0; i < button_count; ++i) {
        push_colored_ui_box(&buttons[i].box);
        push_ui_text(&buttons[i].player_count_text);
    }
}

void ui_team_select_input(raw_input_t *raw_input, event_submissions_t *events) {
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

    if (raw_input->buttons[BT_MOUSE_LEFT].instant) {
        int32_t team = hovered_button;

        if (team > -1) {
            // Check if we can join this team (if it is full compared to the other teams)
            team_color_t color = buttons[team].color;

            if (game_check_team_joinable(color)) {
                // Submit even saying that player just joined team
                LOG_INFO("Local client trying to join team\n");

                event_send_server_team_select_request_t *d = FL_MALLOC(event_send_server_team_select_request_t, 1);
                d->color = color;
                submit_event(ET_SEND_SERVER_TEAM_SELECT_REQUEST, d, events);

                player_t *p = get_player(wd_get_local_player());
                // Need to set team color
                p->flags.team_color = color;

                game_add_player_to_team(p, color);

                ui_init_game_menu_for_server();
            }
        }
    }
}
