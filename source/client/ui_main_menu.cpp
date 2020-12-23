#include "ui_core.hpp"
#include "ui_menu_layout.hpp"
#include "ui_list.hpp"
#include <cstddef>
#include <common/map.hpp>
#include <common/net.hpp>
#include <common/math.hpp>
#include <common/event.hpp>
#include <app.hpp>
#include <vk.hpp>

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

static ui_list_t servers_list_menu;
static ui_list_t build_map_list_menu;

void ui_refresh_main_menu_server_page() {
    available_servers_t *servers = &g_net_data.available_servers;

    ui_list_clear(&servers_list_menu);
    ui_list_begin(&servers_list_menu, servers->server_count);

    for (uint32_t i = 0; i < servers->server_count; ++i) {
        ui_list_add(&servers_list_menu, &servers->servers[i]);
    }

    ui_list_end(&servers_list_menu);
}

static void s_browse_server_menu_init() {
    const char *button_texts[] = { "Connect", "Refresh" };

    void (* handle_input_procs[2])(ui_list_t *, event_submissions_t *) = {
        [] (ui_list_t *list, event_submissions_t *events) {
            if (list->selected_item != 0xFFFF) {
                event_data_request_to_join_server_t *event_data = FL_MALLOC(event_data_request_to_join_server_t, 1);
                void *item_data = list->items[list->selected_item].data;
                game_server_t *server = (game_server_t *)item_data;

                event_data->server_name = server->server_name;

                // Need to close the main menu, and start a fade effect
                event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = ET_REQUEST_TO_JOIN_SERVER;
                effect_data->triggers[0].next_event_data = event_data;
                submit_event(ET_BEGIN_FADE, effect_data, events);
            }
            else if (list->typing_box.is_typing) {
                event_data_request_to_join_server_t *data = FL_MALLOC(event_data_request_to_join_server_t, 1);

                memset(data, 0, sizeof(event_data_request_to_join_server_t));
                data->ip_address = list->typing_box.input_text.get_string();
                // Need to close the main menu, and start a fade effect

                event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = ET_REQUEST_TO_JOIN_SERVER;
                effect_data->triggers[0].next_event_data = data;
                submit_event(ET_BEGIN_FADE, effect_data, events);
            }

            list->typing_box.is_typing = 0;
        },

        [] (ui_list_t *list, event_submissions_t *events) {
            submit_event(ET_REQUEST_REFRESH_SERVER_PAGE, NULL, events);
            list->typing_box.is_typing = 0;
        }
    };

    ui_list_init(
        &main_menu_layout.current_menu,
        &servers_list_menu,
        2, button_texts,
        handle_input_procs,
        [] (ui_list_item_t *item) {
            game_server_t *server = (game_server_t *)item->data;
            item->text.draw_string(server->server_name, 0xFFFFFFFF);
            item->text.null_terminate();
        });
}

void ui_refresh_build_menu_page() {
    map_names_t *map_names = get_map_names();

    ui_list_clear(&build_map_list_menu);
    ui_list_begin(&build_map_list_menu, map_names->count);

    for (uint32_t i = 0; i < map_names->count; ++i) {
        ui_list_add(&build_map_list_menu, &map_names->maps[i]);
    }

    ui_list_end(&build_map_list_menu);
}

static void s_build_map_menu_init() {
    const char *button_texts[] = { "Edit" };

    void (* handle_input_procs[1])(ui_list_t *, event_submissions_t *) = {
        [] (ui_list_t *list, event_submissions_t *events) {
            if (list->selected_item != 0xFFFF) {
                event_enter_map_creator_t *event_data = FL_MALLOC(event_enter_map_creator_t, 1);
                // Data contains the path to the map
                void *item_data = list->items[list->selected_item].data;
                map_names_t::pair_t *pair = (map_names_t::pair_t *)item_data;

                event_data->map_name = pair->name;
                event_data->map_path = pair->path;

                // Need to close the main menu, and start a fade effect
                event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = ET_ENTER_MAP_CREATOR_SCENE;
                effect_data->triggers[0].next_event_data = event_data;
                submit_event(ET_BEGIN_FADE, effect_data, events);
            }
            else if (list->typing_box.is_typing) {
                // Code for the "Edit" button
                const char *path = list->typing_box.input_text.get_string();

                event_enter_map_creator_t *data = FL_MALLOC(event_enter_map_creator_t, 1);
                memset(data, 0, sizeof(event_enter_map_creator_t));
                data->map_name = NULL;
                data->map_path = list->typing_box.input_text.get_string();

                event_begin_fade_effect_t *effect_data = FL_MALLOC(event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = ET_ENTER_MAP_CREATOR_SCENE;
                effect_data->triggers[0].next_event_data = data;
                submit_event(ET_BEGIN_FADE, effect_data, events);
            }
        },
    };

    ui_list_init(
        &main_menu_layout.current_menu,
        &build_map_list_menu,
        1, button_texts,
        handle_input_procs,
        [] (ui_list_item_t *item) {
            map_names_t::pair_t *pair = (map_names_t::pair_t *)item->data;
            item->text.draw_string(pair->name, 0xFFFFFFFF);
            item->text.null_terminate();
        });

    ui_refresh_build_menu_page();
}

static void s_menus_init() {
    // Here initialise all the different menus (browse server, build map, settings, etc...)
    s_browse_server_menu_init();
    s_build_map_menu_init();
}

static void s_menu_layout_quit_proc(
    event_submissions_t *events) {
    submit_event(ET_CLOSED_WINDOW, NULL, events);
}

void ui_main_menu_init() {
    const char *widget_icon_paths[] = { NULL, NULL, NULL, NULL };

    VkDescriptorSet sets[] = {
        ui_texture(UT_PLAY_ICON),
        ui_texture(UT_BUILD_ICON),
        ui_texture(UT_SETTINGS_ICON),
        ui_texture(UT_QUIT_ICON),
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

void ui_submit_main_menu() {
    main_menu_layout.submit();

    if (main_menu_layout.menu_opened()) {
        switch(main_menu_layout.current_open_menu) {
        case B_BROWSE_SERVER: {
            ui_submit_list(&servers_list_menu);
        } break;

        case B_BUILD_MAP: {
            ui_submit_list(&build_map_list_menu);
        } break;
        }
    }
}
    
void ui_main_menu_input(
    event_submissions_t *events,
    const app::raw_input_t *input) {
    if (main_menu_layout.input(events, input)) {
        switch (main_menu_layout.current_open_menu) {
        case B_BROWSE_SERVER: {
            ui_list_input(&servers_list_menu, events, input);
        } break;

        case B_BUILD_MAP: {
            ui_list_input(&build_map_list_menu, events, input);
        } break;
        }
    }
}

void ui_clear_main_menu() {
    servers_list_menu.selected_item = 0xFFFF;
    main_menu_layout.current_button = B_INVALID_MENU_BUTTON;
    main_menu_layout.current_open_menu = B_INVALID_MENU_BUTTON;
    main_menu_layout.current_menu.gls_current_size.fx = 0.0f;
}
