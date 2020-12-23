#include "ui_hover.hpp"
#include <common/game.hpp>
#include "common/event.hpp"
#include "wd_predict.hpp"
#include "ui_list.hpp"
#include <common/allocators.hpp>
#include "ui_hud.hpp"
#include "ui_core.hpp"
#include <common/event.hpp>
#include "ui_menu_layout.hpp"
#include <cstdio>
#include <app.hpp>
#include <common/log.hpp>
#include <cstring>
#include <vk.hpp>

// HUD for now, just contains a crosshair
static ui_box_t crosshair;
static VkDescriptorSet crosshair_texture;

static bool gameplay_display;

static struct {
    ui_box_t box;
    ui_text_t text;
    uint32_t current_health;
} health_display;

static struct {
    ui_box_t box;
    ui_text_t text;
} ammo_display;

static bool display_minibuffer;
static typing_box_t minibuffer;

static bool display_color_table;
static menu_widget_t table_widget;

static struct {
    uint32_t current_crosshair;
    vector2_t uvs[6];
} crosshair_selection;

enum button_t { B_SIGNUP, B_LOGIN, B_USERNAME, B_PASSWORD, B_INVALID };

static ui_box_t panel_box;

static void s_panel_init() {
    panel_box.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.6f, 0.6f),
        NULL,
        0x000000EE);
}

static ui_box_t signup_box;
static ui_text_t signup_text;
static widget_color_t signup_color;

static ui_box_t login_box;
static ui_text_t login_text;
static widget_color_t login_color;

static struct {
    // When you select a server, press connect to request connection
    ui_box_t connect_button;
    ui_text_t connect_text;
    widget_color_t connect_color;

    ui_box_t refresh_button;
    ui_text_t refresh_text;
    widget_color_t refresh_color;

    ui_box_t server_list_box;

    uint32_t selected_server = 0xFFFF;

    bool typing_ip_address;
    ui_box_t ip_address_box;
    ui_input_text_t ip_address;
    widget_color_t ip_address_color;
} browse_server_menu;

void ui_hud_init() {
    { // Crosshair
        crosshair.init(
            RT_CENTER,
            1.0f,
            ui_vector2_t(0.0f, 0.0f),
            ui_vector2_t(0.05f, 0.05f),
            NULL,
            0xFFFFFFFF);

        crosshair_texture = ui_texture(UT_CROSSHAIRS);
        crosshair_selection.current_crosshair = 1;
    }

    { // Health / ammo display
        health_display.box.init(
            RT_LEFT_DOWN,
            1.8f,
            ui_vector2_t(0.05f, 0.05f),
            ui_vector2_t(0.08f, 0.08f),
            NULL,
            0x22222299);

        health_display.text.init(
            &health_display.box,
            ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.7f,
            3,
            2.2f);

        for (uint32_t i = 0; i < 10; ++i) {
            health_display.text.colors[i] = 0xFFFFFFFF;
        }

        gameplay_display = 0;
    }

    { // Minibuffer
        display_minibuffer = 0;

        // Actually initialise minibuffer UI components
        minibuffer.box.init(
            RT_LEFT_DOWN,
            13.0f,
            ui_vector2_t(0.02f, 0.03f),
            ui_vector2_t(0.6f, 0.17f),
            NULL,
            0x09090936);

        minibuffer.input_text.text.init(
            &minibuffer.box,
            ui_game_font(),
            ui_text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.7f,
            38,
            1.8f);

        minibuffer.color.init(
            0x09090936,
            MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
            0xFFFFFFFF,
            0xFFFFFFFF);

        minibuffer.input_text.text_color = 0xFFFFFFFF;
        minibuffer.input_text.cursor_position = 0;
        minibuffer.is_typing = 0;
    }

    ui_color_table_init();
}

void ui_submit_hud() {
    float unit = (1.0f / 8.0f);

    vector2_t start = vector2_t(crosshair_selection.current_crosshair % 8, crosshair_selection.current_crosshair / 8) / 8.0f;
    crosshair_selection.uvs[0] = vector2_t(start.x, start.y + unit);
    crosshair_selection.uvs[1] = crosshair_selection.uvs[3] = vector2_t(start.x, start.y);
    crosshair_selection.uvs[2] = crosshair_selection.uvs[4] = vector2_t(start.x + unit, start.y + unit);
    crosshair_selection.uvs[5] = vector2_t(start.x + unit, start.y);

    mark_ui_textured_section(crosshair_texture);

    push_textured_ui_box(&crosshair, crosshair_selection.uvs);

    if (gameplay_display) {
        // Check if main player health has changed
        int32_t p_idx = wd_get_local_player();
        player_t *p = g_game->get_player(p_idx);

        if (p) {
            uint32_t current_health = p->health;
            if (current_health != health_display.current_health) {
                health_display.current_health = current_health;

                sprintf(health_display.text.characters, "%d", health_display.current_health);
                health_display.text.char_count = strlen(health_display.text.characters);
                health_display.text.null_terminate();
            }

            push_color_ui_box(&health_display.box);
            mark_ui_textured_section(ui_game_font()->font_img.descriptor);
            push_ui_text(&health_display.text);
        }
    }

    ui_submit_minibuffer();
    ui_submit_color_table();
}

void ui_begin_minibuffer() {
    display_minibuffer = 1;
}

void ui_end_minibuffer() {
    display_minibuffer = 0;
}

void ui_minibuffer_update_text(const char *text) {
    if (display_minibuffer) {
        uint32_t str_length = strlen(text);
        for (uint32_t i = 0; i < str_length; ++i) {
            minibuffer.input_text.text.colors[i] = minibuffer.input_text.text_color;
            minibuffer.input_text.text.characters[i] = text[i];
        }

        minibuffer.input_text.cursor_position = str_length;
        minibuffer.input_text.text.char_count = str_length;
        minibuffer.input_text.text.null_terminate();
    }
}

void ui_submit_minibuffer() {
    if (display_minibuffer) {
        ui_submit_typing_box(&minibuffer);
    }
}

void ui_color_table_init() {
    display_color_table = 0;

    table_widget.box.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.7f, 0.7f),
        NULL,
        MENU_WIDGET_NOT_HOVERED_OVER_BACKGROUND_COLOR);

    table_widget.image_box.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.9f, 0.9f),
        &table_widget.box,
        0x000000FF);
}
 
void ui_begin_gameplay_display() {
    gameplay_display = 1;
}

void ui_end_gameplay_display() {
    gameplay_display = 0;
}

void ui_begin_color_table() {
    display_color_table = 1;
}

void ui_end_color_table() {
    display_color_table = 0;
}

void ui_submit_color_table() {
    if (display_color_table) {
        push_color_ui_box(&table_widget.box);
        mark_ui_textured_section(ui_texture(UT_COLOR_TABLE));
        push_textured_ui_box(&table_widget.image_box);
    }
}

void ui_hud_input(event_submissions_t *events, raw_input_t *input) {
    if (display_color_table) {
        if (input->buttons[BT_MOUSE_LEFT].instant) {
            vector2_t color = vector2_t(0.0f);

            // Check where user clicked
            if (ui_hover_over_box(
                    &table_widget.image_box,
                    vector2_t(input->cursor_pos_x, input->cursor_pos_y),
                    &color)) {
                color.y = 1.0f - color.y;

                // Red
                float r_x = floor(fmod(color.x * 16.0f, 8.0f));

                // Green
                float g_y = floor(fmod(color.y * 16.0f, 8.0f));

                // Blue
                float b = floor(color.x * 2.0f) + floor(color.y * 2.0f) * 2.0f;

                event_map_editor_chose_color_t *data = FL_MALLOC(event_map_editor_chose_color_t, 1);
                data->r = (uint8_t)r_x;
                data->g = (uint8_t)g_y;
                data->b = (uint8_t)b;

                submit_event(ET_MAP_EDITOR_CHOSE_COLOR, data, events);
            }
        }
    }
}
