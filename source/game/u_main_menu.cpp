#include "ui.hpp"
#include <renderer/renderer.hpp>

static enum button_t {
    B_BROWSE_SERVER,
    B_BUILD_MAP,
    B_SETTINGS,
    B_QUIT,
    B_INVALID_MENU_BUTTON
} current_button;

static const char *button_names[5] = {
    "BROWSE_SERVER",
    "BUILD_MAP",
    "SETTINGS",
    "QUIT",
    "INVALID"
};

static struct widget_t {
    texture_t texture;
    // Outline of where the texture will be (to add some padding)
    ui_box_t box;
    // Box where the image will be rendered to
    ui_box_t image_box;
} widgets[B_INVALID_MENU_BUTTON];

// This won't be visible
static ui_box_t main_box;

static void s_widget_init(
    button_t button,
    const char *image_path,
    float current_button_y,
    float button_size) {
    widgets[button].box.init(
        RT_RIGHT_UP,
        1.0f,
        ui_vector2_t(0.0f, current_button_y),
        ui_vector2_t(button_size, button_size),
        &main_box,
        0x16161636);

    widgets[button].image_box.init(
        RT_RELATIVE_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        &widgets[button].box,
        0xFFFFFF36);
    
    widgets[button].texture = create_texture(
        image_path,
        VK_FORMAT_R8G8B8A8_UNORM,
        NULL,
        0,
        0,
        VK_FILTER_LINEAR);
}

void u_main_menu_init() {
    main_box.init(
        RT_CENTER,
        2.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        NULL,
        0x46464646);

    float button_size = 0.25f;
    float current_button_y = 0.0f;

    s_widget_init(
        B_BROWSE_SERVER,
        "assets/textures/gui/play_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;
    
    s_widget_init(
        B_BUILD_MAP,
        "assets/textures/gui/build_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;

    s_widget_init(
        B_SETTINGS,
        "assets/textures/gui/settings_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;

    s_widget_init(
        B_QUIT,
        "assets/textures/gui/quit_icon.png",
        current_button_y,
        button_size);
    current_button_y -= button_size;
}

void u_submit_main_menu() {
    for (uint32_t i = 0; i < B_INVALID_MENU_BUTTON; ++i) {
        mark_ui_textured_section(widgets[i].texture.descriptor);
        push_textured_ui_box(&widgets[i].image_box);

        push_colored_ui_box(&widgets[i].box);
    }
}
    
void u_main_menu_input() {

}
