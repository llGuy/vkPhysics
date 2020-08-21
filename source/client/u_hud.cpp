#include "ui.hpp"
#include "u_internal.hpp"
#include <renderer/renderer.hpp>

// HUD for now, just contains a crosshair
static ui_box_t crosshair;
static VkDescriptorSet crosshair_texture;

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

void u_hud_init() {
    crosshair.init(
        RT_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.05f, 0.05f),
        NULL,
        0xFFFFFFFF);

    crosshair_texture = u_texture(UT_CROSSHAIRS);

    crosshair_selection.current_crosshair = 1;
}

void u_submit_hud() {
    float unit = (1.0f / 8.0f);

    vector2_t start = vector2_t(crosshair_selection.current_crosshair % 8, crosshair_selection.current_crosshair / 8) / 8.0f;
    crosshair_selection.uvs[0] = vector2_t(start.x, start.y + unit);
    crosshair_selection.uvs[1] = crosshair_selection.uvs[3] = vector2_t(start.x, start.y);
    crosshair_selection.uvs[2] = crosshair_selection.uvs[4] = vector2_t(start.x + unit, start.y + unit);
    crosshair_selection.uvs[5] = vector2_t(start.x + unit, start.y);

    mark_ui_textured_section(crosshair_texture);

    push_textured_ui_box(&crosshair, crosshair_selection.uvs);
}
