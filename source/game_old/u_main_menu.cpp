#include "ui.hpp"
#include "net.hpp"
#include "u_internal.hpp"
#include <common/math.hpp>
#include <common/event.hpp>
#include <cstddef>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

enum button_t {
    B_BROWSE_SERVER,
    B_BUILD_MAP,
    B_SETTINGS,
    B_QUIT,
    B_INVALID_MENU_BUTTON
};

static const char *button_names[5] = {
    "BROWSE_SERVER",
    "BUILD_MAP",
    "SETTINGS",
    "QUIT",
    "INVALID"
};

static menu_layout_t main_menu_layout;

struct server_button_t {
    ui_box_t box;
    ui_text_t server_name;

    widget_color_t button_color;

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

    uint32_t selected_server = 0xFFFF;

    bool typing_ip_address;
    ui_box_t ip_address_box;
    ui_input_text_t ip_address;
    widget_color_t ip_address_color;
} browse_server_menu;

static struct {
    ui_box_t edit_button;
    ui_text_t edit_text;
} build_map_menu;

void u_refresh_main_menu_server_page() {
    available_servers_t *servers = get_available_servers();

    browse_server_menu.server_count = servers->server_count;

    float server_button_height = 0.1f;

    if (browse_server_menu.server_count) {
        if (browse_server_menu.servers) {
            FL_FREE(browse_server_menu.servers);
        }

        browse_server_menu.servers = FL_MALLOC(server_button_t, browse_server_menu.server_count);

        for (uint32_t i = 0; i < browse_server_menu.server_count; ++i) {
            server_button_t *current_button = &browse_server_menu.servers[i];

            current_button->server = &servers->servers[i];

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
                servers->servers[i].server_name,
                0xFFFFFFFF);

            current_button->server_name.null_terminate();

            current_button->button_color.init(
                0x05050536,
                MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
                0xFFFFFFFF,
                0xFFFFFFFF);
        }
    }
}

static void s_browse_server_menu_init() {
    browse_server_menu.server_list_box.init(
        RT_RIGHT_DOWN,
        2.1f,
        ui_vector2_t(-0.02f, 0.15f),
        ui_vector2_t(0.95f, 0.95f), &main_menu_layout.current_menu, 0x09090936);

    // CONNECT_BUTTON ////////////////////////////////////////////////////////
    browse_server_menu.connect_button.init(
        RT_RIGHT_DOWN,
        4.0f,
        ui_vector2_t(-0.02f, 0.03f),
        ui_vector2_t(0.2f, 0.2f),
        &main_menu_layout.current_menu,
        0x09090936);

    browse_server_menu.connect_text.init(
        &browse_server_menu.connect_button,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f, 0.9f,
        10, 1.8f);

    if (!browse_server_menu.connect_text.char_count) {
        browse_server_menu.connect_text.draw_string(
            "Connect",
            0xFFFFFFFF);
    }

    browse_server_menu.connect_color.init(
        0x09090936,
        MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
        0xFFFFFFFF,
        0xFFFFFFFF);

    browse_server_menu.connect_text.null_terminate();

    // REFRESH BUTTON /////////////////////////////////////////////////////////
    browse_server_menu.refresh_button.init(
        RT_RIGHT_DOWN,
        4.0f,
        ui_vector2_t(-0.25f, 0.03f),
        ui_vector2_t(0.2f, 0.2f),
        &main_menu_layout.current_menu,
        0x09090936);

    browse_server_menu.refresh_text.init(
        &browse_server_menu.refresh_button,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f, 0.9f,
        10, 1.8f);

    if (!browse_server_menu.refresh_text.char_count) {
        browse_server_menu.refresh_text.draw_string(
            "Refresh",
            0xFFFFFFFF);
    }

    browse_server_menu.refresh_color.init(
        0x09090936,
        MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
        0xFFFFFFFF,
        0xFFFFFFFF);

    browse_server_menu.refresh_text.null_terminate();

    // SERVERS ////////////////////////////////////////////////////////////////
    u_refresh_main_menu_server_page();

    browse_server_menu.ip_address_box.init(
        RT_LEFT_DOWN,
        6.0f,
        ui_vector2_t(0.02f, 0.03f),
        ui_vector2_t(0.28f, 0.2f),
        &main_menu_layout.current_menu,
        0x09090936);

    browse_server_menu.ip_address.text.init(
        &browse_server_menu.ip_address_box,
        u_game_font(),
        ui_text_t::font_stream_box_relative_to_t::BOTTOM,
        0.8f,
        0.9f,
        18,
        1.8f);

    browse_server_menu.ip_address_color.init(
        0x09090936,
        MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR,
        0xFFFFFFFF,
        0xFFFFFFFF);

    browse_server_menu.ip_address.text_color = 0xFFFFFFFF;

    browse_server_menu.ip_address.cursor_position = 0;

    browse_server_menu.typing_ip_address = 0;
}

static void s_menus_init() {
    // Here initialise all the different menus (browse server, build map, settings, etc...)
    s_browse_server_menu_init();
}

static void s_menu_layout_quit_proc(
    event_submissions_t *events) {
    submit_event(ET_CLOSED_WINDOW, NULL, events);
}

void u_main_menu_init() {
    const char *widget_icon_paths[] = { NULL, NULL, NULL, NULL };

    VkDescriptorSet sets[] = {
        u_texture(UT_PLAY_ICON),
        u_texture(UT_BUILD_ICON),
        u_texture(UT_SETTINGS_ICON),
        u_texture(UT_QUIT_ICON),
    };

    menu_click_handler_t procs[] = {
        NULL,
        NULL,
        NULL,
        &s_menu_layout_quit_proc
    };

    main_menu_layout.init(procs, widget_icon_paths, sets, B_INVALID_MENU_BUTTON);

    s_menus_init();
}

void u_submit_main_menu() {
    main_menu_layout.submit();

    if (main_menu_layout.menu_opened()) {
        switch(main_menu_layout.current_open_menu) {
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

            push_ui_input_text(1, 0xFFFFFFFF, &browse_server_menu.ip_address);

            push_colored_ui_box(
                &browse_server_menu.ip_address_box);

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
    
static void s_browse_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    // IP ADDRESS TYPING //////////////////////////////////////////////////////
    bool hovered_over_ip_address = u_hover_over_box(
        &browse_server_menu.ip_address_box,
        input->cursor_pos_x,
        input->cursor_pos_y);

    color_pair_t pair = browse_server_menu.ip_address_color.update(
        MENU_WIDGET_HOVER_COLOR_FADE_SPEED,
        hovered_over_ip_address);

    browse_server_menu.ip_address_box.color = pair.current_background;

    if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_ip_address) {
        browse_server_menu.typing_ip_address = 1;

        browse_server_menu.selected_server = 0xFFFF;
    }

    if (browse_server_menu.typing_ip_address) {
        browse_server_menu.ip_address.input(
            input);
    }

    // CONNECT BUTTON /////////////////////////////////////////////////////////
    bool hovered_over_connect = u_hover_over_box(
        &browse_server_menu.connect_button,
        input->cursor_pos_x,
        input->cursor_pos_y);

    pair = browse_server_menu.connect_color.update(
        MENU_WIDGET_HOVER_COLOR_FADE_SPEED,
        hovered_over_connect);

    browse_server_menu.connect_button.color = pair.current_background;

    if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_connect) {
        if (browse_server_menu.selected_server != 0xFFFF) {
            event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);
            game_server_t *server = browse_server_menu.servers[browse_server_menu.selected_server].server;

            data->server_name = server->server_name;
            submit_event(ET_REQUEST_TO_JOIN_SERVER, data, events);
            // Need to close the main menu, and start a fade effect
            submit_event(ET_CLEAR_MENUS, NULL, events);

            event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
            effect_data->dest_value = 0.0f;
            effect_data->duration = 2.5f;
            effect_data->fade_back = 1;
            effect_data->trigger_count = 3;
            effect_data->triggers[0].trigger_type = ET_LAUNCH_GAME_MENU_SCREEN;
            effect_data->triggers[0].next_event_data = NULL;
            effect_data->triggers[1].trigger_type = ET_ENTER_SERVER_META_MENU;
            effect_data->triggers[1].next_event_data = NULL;
            effect_data->triggers[2].trigger_type = ET_EXIT_MAIN_MENU_SCREEN;
            effect_data->triggers[2].next_event_data = NULL;
            submit_event(ET_BEGIN_FADE, effect_data, events);
        }
        else if (browse_server_menu.typing_ip_address) {
            event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);

            memset(data, 0, sizeof(event_data_request_to_join_server_t));
            data->ip_address = browse_server_menu.ip_address.get_string();
            submit_event(ET_REQUEST_TO_JOIN_SERVER, data, events);
            // Need to close the main menu, and start a fade effect
            submit_event(ET_CLEAR_MENUS, NULL, events);

            event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
            effect_data->dest_value = 0.0f;
            effect_data->duration = 2.5f;
            effect_data->fade_back = 1;
            effect_data->trigger_count = 3;
            effect_data->triggers[0].trigger_type = ET_LAUNCH_GAME_MENU_SCREEN;
            effect_data->triggers[0].next_event_data = NULL;
            effect_data->triggers[1].trigger_type = ET_ENTER_SERVER_META_MENU;
            effect_data->triggers[1].next_event_data = NULL;
            effect_data->triggers[2].trigger_type = ET_EXIT_MAIN_MENU_SCREEN;
            effect_data->triggers[2].next_event_data = NULL;
            submit_event(ET_BEGIN_FADE, effect_data, events);
        }

        browse_server_menu.typing_ip_address = 0;
    }

    // REFRESH BUTTON /////////////////////////////////////////////////////////
    bool hovered_over_refresh = u_hover_over_box(
        &browse_server_menu.refresh_button,
        input->cursor_pos_x,
        input->cursor_pos_y);

    pair = browse_server_menu.refresh_color.update(
        MENU_WIDGET_HOVER_COLOR_FADE_SPEED,
        hovered_over_refresh);

    browse_server_menu.refresh_button.color = pair.current_background;

    if (input->buttons[BT_MOUSE_LEFT].instant && hovered_over_refresh) {
        submit_event(ET_REQUEST_REFRESH_SERVER_PAGE, NULL, events);

        browse_server_menu.typing_ip_address = 0;
    }

    // SERVER SELECTION ///////////////////////////////////////////////////////
    for (uint32_t i = 0; i < browse_server_menu.server_count; ++i) {
        server_button_t *button = &browse_server_menu.servers[i];

        if (i == browse_server_menu.selected_server) {
            button->box.color = MENU_WIDGET_HOVERED_OVER_BACKGROUND_COLOR;
        }
        else {
            bool hovered_over_server = u_hover_over_box(
                &button->box,
                input->cursor_pos_x,
                input->cursor_pos_y);

            pair = button->button_color.update(
                MENU_WIDGET_HOVER_COLOR_FADE_SPEED,
                hovered_over_server);

            button->box.color = pair.current_background;

            if (hovered_over_server && input->buttons[BT_MOUSE_LEFT].instant) {
                browse_server_menu.selected_server = i;

                browse_server_menu.typing_ip_address = 0;
            }
        }
    }
}

void u_main_menu_input(
    event_submissions_t *events,
    raw_input_t *input) {
    if (main_menu_layout.input(events, input)) {
        switch (main_menu_layout.current_open_menu) {
        case B_BROWSE_SERVER: {
            s_browse_menu_input(
                events,
                input);
        } break;
        }
    }
}

void u_clear_main_menu() {
    browse_server_menu.selected_server = 0xFFFF;
    main_menu_layout.current_button = B_INVALID_MENU_BUTTON;
    main_menu_layout.current_open_menu = B_INVALID_MENU_BUTTON;
    main_menu_layout.current_menu.gls_current_size.fx = 0.0f;
}
