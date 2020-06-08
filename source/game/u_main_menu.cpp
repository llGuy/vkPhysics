#include "ui.hpp"
#include "net.hpp"
#include "u_internal.hpp"
#include <common/math.hpp>
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

static enum button_t {
    B_BROWSE_SERVER,
    B_BUILD_MAP,
    B_SETTINGS,
    B_QUIT,
    B_INVALID_MENU_BUTTON
} current_button, current_open_menu;

static const char *button_names[5] = {
    "BROWSE_SERVER",
    "BUILD_MAP",
    "SETTINGS",
    "QUIT",
    "INVALID"
};

#define NOT_HOVERED_OVER_ICON_COLOR 0xFFFFFF36
#define NOT_HOVERED_OVER_BACKGROUND_COLOR 0x16161636
// #define HOVERED_OVER_ICON_COLOR 0x46464636
#define HOVERED_OVER_ICON_COLOR NOT_HOVERED_OVER_ICON_COLOR
#define HOVERED_OVER_BACKGROUND_COLOR 0x76767636
#define CURRENT_MENU_BACKGROUND 0x13131336
#define HOVER_COLOR_FADE_SPEED 0.3f

static struct widget_t {
    texture_t texture;
    // Outline of where the texture will be (to add some padding)
    ui_box_t box;
    // Box where the image will be rendered to
    ui_box_t image_box;

    widget_color_t color;

    bool hovered_on;
} widgets[B_INVALID_MENU_BUTTON];

// This won't be visible
static ui_box_t main_box;

static ui_box_t current_menu; // Menu that sliddes
static smooth_linear_interpolation_t menu_slider;
static bool menu_in_out_transition;
static float menu_slider_x_min_size, menu_slider_x_max_size, menu_slider_y_min_size, menu_slider_y_max_size;

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
        NOT_HOVERED_OVER_BACKGROUND_COLOR);

    widgets[button].image_box.init(
        RT_RELATIVE_CENTER,
        1.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        &widgets[button].box,
        NOT_HOVERED_OVER_ICON_COLOR);
    
    widgets[button].texture = create_texture(
        image_path,
        VK_FORMAT_R8G8B8A8_UNORM,
        NULL,
        0,
        0,
        VK_FILTER_LINEAR);

    widgets[button].color.init(
        NOT_HOVERED_OVER_BACKGROUND_COLOR,
        HOVERED_OVER_BACKGROUND_COLOR,
        NOT_HOVERED_OVER_ICON_COLOR,
        HOVERED_OVER_ICON_COLOR);
}

static void s_widgets_init() {
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

struct server_button_t {
    ui_box_t box;
    ui_text_t server_name;

    game_server_t *server;

    widget_color_t color;
};

static struct {
    // When you select a server, press connect to request connection
    ui_box_t connect_button;
    ui_text_t connect_text;
    widget_color_t connect_color;

    ui_box_t refresh_button;
    ui_text_t refresh_text;
    widget_color_t refresh_color;

    ui_box_t server_list_box;

    uint32_t server_count;
    server_button_t *servers;
} browse_server_menu;

static void s_refresh_browse_server_list() {
    game_server_t srv_array[4] = {
        { "Yoshi's Island", 0 },
        { "Bowser's Castle", 0 },
        { "Super Mario Land", 0 },
        { "Super Mario Galaxy", 0 },
    };

    available_servers_t servers;
    servers.server_count = 4;
    servers.servers = srv_array;
    // available_servers_t *servers = get_available_servers();

    browse_server_menu.server_count = servers.server_count;

    float server_button_height = 0.1f;

    if (browse_server_menu.server_count) {
        browse_server_menu.servers = FL_MALLOC(server_button_t, browse_server_menu.server_count);

        for (uint32_t i = 0; i < browse_server_menu.server_count; ++i) {
            server_button_t *current_button = &browse_server_menu.servers[i];
            current_button->box.init(
                RT_LEFT_UP,
                20.8f,
                ui_vector2_t(0.0f, -server_button_height * (float)i),
                ui_vector2_t(1.0f, server_button_height),
                &browse_server_menu.server_list_box,
                0x05050536);

            current_button->server_name.init(
                &current_button->box,
                u_game_font(),
                ui_text_t::font_stream_box_relative_to_t::BOTTOM,
                0.8f,
                0.8f,
                60,
                1.8f);

            current_button->server_name.draw_string(
                srv_array[i].server_name,
                0xFFFFFFFF);

            current_button->server_name.null_terminate();
        }
    }
}

static void s_browse_server_menu_init() {
    browse_server_menu.server_list_box.init(
        RT_RIGHT_DOWN,
        2.1f,
        ui_vector2_t(-0.02f, 0.15f),
        ui_vector2_t(0.95f, 0.95f), &current_menu, 0x09090936);

    // CONNECT_BUTTON ////////////////////////////////////////////////////////
    browse_server_menu.connect_button.init(
        RT_RIGHT_DOWN,
        4.0f,
        ui_vector2_t(-0.02f, 0.03f),
        ui_vector2_t(0.2f, 0.2f),
        &current_menu,
        0x09090936);

    browse_server_menu.connect_text.init(
        &browse_server_menu.connect_button,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f, 0.9f,
        10, 1.8f);

    browse_server_menu.connect_text.draw_string(
        "Connect",
        0xFFFFFFFF);

    browse_server_menu.connect_color.init(
        0x09090936,
        HOVERED_OVER_BACKGROUND_COLOR,
        0xFFFFFFFF,
        0xFFFFFFFF);

    browse_server_menu.connect_text.null_terminate();

    // REFRESH BUTTON /////////////////////////////////////////////////////////
    browse_server_menu.refresh_button.init(
        RT_RIGHT_DOWN,
        4.0f,
        ui_vector2_t(-0.25f, 0.03f),
        ui_vector2_t(0.2f, 0.2f),
        &current_menu,
        0x09090936);

    browse_server_menu.refresh_text.init(
        &browse_server_menu.refresh_button,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f, 0.9f,
        10, 1.8f);

    browse_server_menu.refresh_text.draw_string(
        "Refresh",
        0xFFFFFFFF);

    browse_server_menu.refresh_color.init(
        0x09090936,
        HOVERED_OVER_BACKGROUND_COLOR,
        0xFFFFFFFF,
        0xFFFFFFFF);

    browse_server_menu.refresh_text.null_terminate();

    // SERVERS ////////////////////////////////////////////////////////////////
    s_refresh_browse_server_list();
}

static void s_menus_init() {
    current_menu.init(
        RT_RIGHT_UP,
        1.75f,
        ui_vector2_t(-0.125f, 0.0f),
        ui_vector2_t(1.0f, 1.0f),
        &main_box,
        CURRENT_MENU_BACKGROUND);

    // Here initialise all the different menus (browse server, build map, settings, etc...)
    s_browse_server_menu_init();

    menu_slider_x_max_size = current_menu.gls_current_size.to_fvec2().x;
    menu_slider_y_max_size = current_menu.gls_current_size.to_fvec2().y;

    current_menu.gls_current_size.fx = 0.0f;

    menu_in_out_transition = 0;

    current_open_menu = B_INVALID_MENU_BUTTON;
}

void u_main_menu_init() {
    current_button = B_INVALID_MENU_BUTTON;
    memset(widgets, sizeof(widgets), 0);

    main_box.init(
        RT_CENTER,
        2.0f,
        ui_vector2_t(0.0f, 0.0f),
        ui_vector2_t(0.8f, 0.8f),
        NULL,
        0x46464646);

    s_widgets_init();
    s_menus_init();
}

void u_submit_main_menu() {
    for (uint32_t i = 0; i < B_INVALID_MENU_BUTTON; ++i) {
        mark_ui_textured_section(widgets[i].texture.descriptor);
        push_textured_ui_box(&widgets[i].image_box);

        push_colored_ui_box(&widgets[i].box);
    }

    push_reversed_colored_ui_box(
        &current_menu,
        vector2_t(menu_slider_x_max_size, menu_slider_y_max_size) * 2.0f);

    if (current_open_menu != B_INVALID_MENU_BUTTON && !menu_slider.in_animation) {
        switch(current_open_menu) {
        case B_BROWSE_SERVER: {
            mark_ui_textured_section(u_game_font()->font_img.descriptor);
    
            push_ui_text(
                &browse_server_menu.connect_text);

            push_colored_ui_box(
                &browse_server_menu.connect_button);

            push_colored_ui_box(
                &browse_server_menu.server_list_box);

            push_ui_text(
                &browse_server_menu.refresh_text);

            push_colored_ui_box(
                &browse_server_menu.refresh_button);

            for (uint32_t i = 0; i < browse_server_menu.server_count; ++i) {
                server_button_t *button = &browse_server_menu.servers[i];
                push_colored_ui_box(
                    &button->box);
                push_ui_text(
                    &button->server_name);
            }
        } break;
        }
    }
}
    
static bool s_hover_over_box(
    ui_box_t *box,
    float cursor_x,
    float cursor_y) {
    vector2_t cursor = convert_pixel_to_ndc(vector2_t(cursor_x, cursor_y));

    vector2_t normalized_base_position = convert_glsl_to_normalized(box->gls_position.to_fvec2());
    vector2_t normalized_size = box->gls_current_size.to_fvec2() * 2.0f;

    float x_min = normalized_base_position.x,
        x_max = normalized_base_position.x + normalized_size.x,
        y_min = normalized_base_position.y,
        y_max = normalized_base_position.y + normalized_size.y;

    if (x_min < cursor.x && x_max > cursor.x
        && y_min < cursor.y && y_max > cursor.y) {
        return(true);
    }
    else {
        return(false);
    }
}

static bool s_hover_over_button(
    button_t button,
    float cursor_x,
    float cursor_y) {
    return s_hover_over_box(
        &widgets[button].box,
        cursor_x,
        cursor_y);
}


static void s_widgets_mouse_hover_detection(
    raw_input_t *input) {
    float cursor_x = input->cursor_pos_x, cursor_y = input->cursor_pos_y;
    // Check if user is hovering over any buttons
    bool hovered_over_button = 0;

    for (uint32_t i = 0; i < (uint32_t)B_INVALID_MENU_BUTTON; ++i) {
        bool hovered_over = s_hover_over_button((button_t)i, cursor_x, cursor_y);

        color_pair_t pair = widgets[i].color.update(
            HOVER_COLOR_FADE_SPEED,
            hovered_over);

        if (hovered_over) {
            current_button = (button_t)i;
            hovered_over_button = 1;
            widgets[i].hovered_on = 1;
        }
        else {
            widgets[i].hovered_on = 0;
        }

        widgets[i].image_box.color = pair.current_foreground;
        widgets[i].box.color = pair.current_background;
    }

    if (!hovered_over_button) {
        current_button = B_INVALID_MENU_BUTTON;
    }
}

#define MENU_SLIDER_SPEED 0.3f

static void s_open_menu(
    button_t button) {
    if (button != current_open_menu) {
        if (current_open_menu == B_INVALID_MENU_BUTTON) {
            current_open_menu = button;

            menu_slider.set(
                1,
                menu_slider.current,
                menu_slider_x_max_size,
                MENU_SLIDER_SPEED);
        }
        else {
            current_open_menu = button;

            menu_slider.set(
                1,
                menu_slider.current,
                0.0f,
                MENU_SLIDER_SPEED);

            menu_in_out_transition = 1;
        }
    }
    else {
        current_open_menu = B_INVALID_MENU_BUTTON;

        menu_slider.set(
            1,
            menu_slider.current,
            0.0f,
            MENU_SLIDER_SPEED);
    }
}

static void s_widgets_input(
    event_submissions_t *events,
    raw_input_t *input) {
    if (input->buttons[BT_MOUSE_LEFT].instant) {
        switch (current_button) {
            // Launch / unlaunch menus

        case B_BROWSE_SERVER: case B_BUILD_MAP: case B_SETTINGS: {
            s_open_menu(
                current_button);
        } break;

        case B_QUIT: {
            submit_event(ET_CLOSED_WINDOW, NULL, events);
        } break;

        case B_INVALID_MENU_BUTTON: {
            // Don't do anything
        } break;
        }
    }
}

static void s_browse_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    // CONNECT BUTTON /////////////////////////////////////////////////////////
    bool hovered_over_connect = s_hover_over_box(
        &browse_server_menu.connect_button,
        input->cursor_pos_x,
        input->cursor_pos_y);

    color_pair_t pair = browse_server_menu.connect_color.update(
        HOVER_COLOR_FADE_SPEED,
        hovered_over_connect);

    browse_server_menu.connect_button.color = pair.current_background;

    if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_connect) {
        printf("Pressed connect\n");
    }

    // REFRESH BUTTON /////////////////////////////////////////////////////////
    bool hovered_over_refresh = s_hover_over_box(
        &browse_server_menu.refresh_button,
        input->cursor_pos_x,
        input->cursor_pos_y);

    pair = browse_server_menu.refresh_color.update(
        HOVER_COLOR_FADE_SPEED,
        hovered_over_refresh);

    browse_server_menu.refresh_button.color = pair.current_background;

    if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_refresh) {
        printf("Pressed refresh\n");
    }
}

static void s_open_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    menu_slider.animate(surface_delta_time());
    current_menu.gls_current_size.fx = menu_slider.current;

    if (menu_in_out_transition && !menu_slider.in_animation) {
        menu_in_out_transition = 0;
        
        menu_slider.set(
            1,
            menu_slider.current,
            menu_slider_x_max_size,
            MENU_SLIDER_SPEED);
    }
    else if (current_open_menu != B_INVALID_MENU_BUTTON && !menu_slider.in_animation) {
        switch (current_open_menu) {
        case B_BROWSE_SERVER: {
            s_browse_menu_input(
                events,
                input);
        } break;
        }
    }
}

void u_main_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    s_widgets_mouse_hover_detection(input);
    s_widgets_input(events, input);

    s_open_menu_input(events, input);
}
