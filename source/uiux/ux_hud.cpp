#include "ux_hud.hpp"

#include "containers.hpp"
#include "glm/common.hpp"
#include "ui_math.hpp"
#include "ux.hpp"
#include "ux_hover.hpp"
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_event_data.hpp>
#include "ux_list.hpp"
#include <allocators.hpp>
#include "ux_menu_layout.hpp"
#include "ui_submit.hpp"
#include <cstdio>
#include <app.hpp>
#include <log.hpp>
#include <cstring>
#include <vk.hpp>

namespace ux {

// HUD for now, just contains a crosshair
static ui::box_t crosshair;
static VkDescriptorSet crosshair_texture;

static bool gameplay_display;

static struct {
    ui::box_t box;
    ui::text_t text;
    uint32_t current_health;
} health_display;

static struct {
    ui::box_t box;
    ui::text_t text;
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

static ui::box_t panel_box;

static void s_panel_init() {
    panel_box.init(
        ui::RT_CENTER,
        1.0f,
        ui::vector2_t(0.0f, 0.0f),
        ui::vector2_t(0.6f, 0.6f),
        NULL,
        0x000000EE);
}

static ui::box_t signup_box;
static ui::text_t signup_text;
static widget_color_t signup_color;

static ui::box_t damage_indicator;

static constexpr uint32_t MAX_DAMAGES = 30;

struct damage_t {
    float rotation_angle;
    float time_left;
    bool visible;
};

static static_stack_container_t<damage_t, MAX_DAMAGES> damages;

static ui::box_t login_box;
static ui::text_t login_text;
static widget_color_t login_color;

static struct {
    // When you select a server, press connect to request connection
    ui::box_t connect_button;
    ui::text_t connect_text;
    widget_color_t connect_color;

    ui::box_t refresh_button;
    ui::text_t refresh_text;
    widget_color_t refresh_color;

    ui::box_t server_list_box;

    uint32_t selected_server = 0xFFFF;

    bool typing_ip_address;
    ui::box_t ip_address_box;
    ui::input_text_t ip_address;
    widget_color_t ip_address_color;
} browse_server_menu;

void init_hud() {
    { // Crosshair
        crosshair.init(
            ui::RT_CENTER,
            1.0f,
            ui::vector2_t(0.0f, 0.0f),
            ui::vector2_t(0.05f, 0.05f),
            NULL,
            0xFFFFFFFF);

        crosshair_texture = get_texture(UT_CROSSHAIRS);
        crosshair_selection.current_crosshair = 1;
    }

    { // Health / ammo display
        health_display.box.init(
            ui::RT_LEFT_DOWN,
            1.8f,
            ui::vector2_t(0.05f, 0.05f),
            ui::vector2_t(0.08f, 0.08f),
            NULL,
            0x22222299);

        health_display.text.init(
            &health_display.box,
            get_game_font(),
            ui::text_t::font_stream_box_relative_to_t::BOTTOM,
            0.8f,
            0.7f,
            3,
            2.2f);

        for (uint32_t i = 0; i < 10; ++i) {
            health_display.text.colors[i] = 0xFFFFFFFF;
        }

        gameplay_display = 0;
    }

    { // Damage indicator
        damage_indicator.init(
            ui::RT_CENTER, 1.0f,
            ui::vector2_t(0.0f, 0.0f),
            ui::vector2_t(0.7f, 0.7f),
            NULL,
            0xFF0000FF);

        damages.init();
    }

    { // Minibuffer
        display_minibuffer = 0;

        // Actually initialise minibuffer UI components
        minibuffer.box.init(
            ui::RT_LEFT_DOWN,
            13.0f,
            ui::vector2_t(0.02f, 0.03f),
            ui::vector2_t(0.6f, 0.17f),
            NULL,
            0x09090936);

        minibuffer.input_text.text.init(
            &minibuffer.box,
            get_game_font(),
            ui::text_t::font_stream_box_relative_to_t::BOTTOM,
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

    color_table_init();
}

void submit_hud(const vkph::state_t *state) {
    float unit = (1.0f / 8.0f);

    vector2_t start = vector2_t(crosshair_selection.current_crosshair % 8, crosshair_selection.current_crosshair / 8) / 8.0f;
    crosshair_selection.uvs[0] = vector2_t(start.x, start.y + unit);
    crosshair_selection.uvs[1] = crosshair_selection.uvs[3] = vector2_t(start.x, start.y);
    crosshair_selection.uvs[2] = crosshair_selection.uvs[4] = vector2_t(start.x + unit, start.y + unit);
    crosshair_selection.uvs[5] = vector2_t(start.x + unit, start.y);

    ui::mark_ui_textured_section(crosshair_texture);

    ui::push_textured_box(&crosshair, crosshair_selection.uvs);

    if (gameplay_display) {
        // Check if main player health has changed
        int32_t p_idx = state->local_player_id;
        const vkph::player_t *p = state->get_player(p_idx);

        if (p) {
            uint32_t current_health = p->health;
            if (current_health != health_display.current_health) {
                health_display.current_health = current_health;

                sprintf(health_display.text.characters, "%d", health_display.current_health);
                health_display.text.char_count = (uint32_t)strlen(health_display.text.characters);
                health_display.text.null_terminate();
            }

            ui::push_color_box(&health_display.box);
            ui::mark_ui_textured_section(get_game_font()->font_img.descriptor);
            ui::push_text(&health_display.text);
        }
    }

    for (uint32_t i = 0; i < damages.data_count; ++i) {
        uint32_t base_color = 0xFF060600;

        damage_t *d = &damages[i];

        if (d->time_left < 0.0f) {
            d->visible = 0;
            damages.remove(i);

            printf("%d\n", damages.data_count);
        }

        if (d->visible) {
            base_color += (uint32_t)(255.0f * d->time_left);
            
            damage_indicator.rotation_angle = d->rotation_angle;

            ui::mark_ui_textured_section(get_texture(UT_DAMAGE_INDICATOR));
            ui::push_textured_box(&damage_indicator, base_color);

            d->time_left -= app::g_delta_time;
        }
    }

    submit_minibuffer();
    submit_color_table();
}

void begin_minibuffer() {
    display_minibuffer = 1;
}

void end_minibuffer() {
    display_minibuffer = 0;
}

void minibuffer_update_text(const char *text) {
    if (display_minibuffer) {
        uint32_t str_length = (uint32_t)strlen(text);
        for (uint32_t i = 0; i < str_length; ++i) {
            minibuffer.input_text.text.colors[i] = minibuffer.input_text.text_color;
            minibuffer.input_text.text.characters[i] = text[i];
        }

        minibuffer.input_text.cursor_position = str_length;
        minibuffer.input_text.text.char_count = str_length;
        minibuffer.input_text.text.null_terminate();
    }
}

void submit_minibuffer() {
    if (display_minibuffer) {
        ui::push_color_box(&minibuffer.box);
        ui::mark_ui_textured_section(get_game_font()->font_img.descriptor);
        ui::push_ui_input_text(1, 0, 0xFFFFFFFF, &minibuffer.input_text);
    }
}

void color_table_init() {
    display_color_table = 0;

    table_widget.box.init(
        ui::RT_CENTER,
        1.0f,
        ui::vector2_t(0.0f, 0.0f),
        ui::vector2_t(0.7f, 0.7f),
        NULL,
        MENU_WIDGET_NOT_HOVERED_OVER_BACKGROUND_COLOR);

    table_widget.image_box.init(
        ui::RT_CENTER,
        1.0f,
        ui::vector2_t(0.0f, 0.0f),
        ui::vector2_t(0.9f, 0.9f),
        &table_widget.box,
        0x000000FF);
}
 
void begin_gameplay_display() {
    gameplay_display = 1;
}

void end_gameplay_display() {
    gameplay_display = 0;
}

void begin_color_table() {
    display_color_table = 1;
}

void end_color_table() {
    display_color_table = 0;
}

void submit_color_table() {
    if (display_color_table) {
        ui::push_color_box(&table_widget.box);
        ui::mark_ui_textured_section(get_texture(UT_COLOR_TABLE));
        ui::push_textured_box(&table_widget.image_box);
    }
}

void hud_input(const app::raw_input_t *input) {
    if (display_color_table) {
        if (input->buttons[app::BT_MOUSE_LEFT].instant) {
            vector2_t color = vector2_t(0.0f);

            // Check where user clicked
            if (is_hovering_over_box(
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

                auto *data = flmalloc<vkph::event_map_editor_chose_color_t>(1);
                data->r = (uint8_t)r_x;
                data->g = (uint8_t)g_y;
                data->b = (uint8_t)b;

                vkph::submit_event(vkph::ET_MAP_EDITOR_CHOSE_COLOR, data);
            }
        }
    }
}

void add_damage_indicator(
    const vector3_t &view_dir,
    const vector3_t &up,
    const vector3_t &right,
    const vector3_t &bullet_dir) {
    damage_t res = {};
    res.time_left = 1.0f;

    matrix3_t inv_basis;
    inv_basis[0] = vector3_t(right.x, up.x, -view_dir.x);
    inv_basis[1] = vector3_t(right.y, up.y, -view_dir.y);
    inv_basis[2] = vector3_t(right.z, up.z, -view_dir.z);

    vector3_t bd = inv_basis * bullet_dir;
    bd.y = 0.0f;

    float d = glm::dot(bd, vector3_t(0.0f, 0.0f, -1.0f));
    d = glm::clamp(d, -1.0f, 1.0f);
    res.rotation_angle = glm::acos(d);

    if (bd.x > 0.0f) {
        res.rotation_angle *= -1.0f;
    }

    uint32_t idx = damages.add();
    auto *dst = &damages[idx];
    *dst = res;
    dst->visible = 1;
}

}
