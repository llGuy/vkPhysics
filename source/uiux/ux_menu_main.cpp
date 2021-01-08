#include "ux.hpp"
#include <net_meta.hpp>
#include "ux_list.hpp"
#include "ux_menu_layout.hpp"
#include "vkph_event.hpp"
#include <vkph_event_data.hpp>
#include <vkph_state.hpp>
#include <vkph_events.hpp>

namespace ux {

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

static list_t servers_list_menu;
static list_t build_map_list_menu;

void refresh_main_menu_server_page() {
    auto *servers = net::get_available_servers();

    servers_list_menu.clear();
    servers_list_menu.begin(servers->server_count);

    for (uint32_t i = 0; i < servers->server_count; ++i) {
        servers_list_menu.add(&servers->servers[i]);
    }

    servers_list_menu.end();
}

static void s_browse_server_menu_init() {
    const char *button_texts[] = { "Connect", "Refresh" };

    void (* handle_input_procs[2])(list_t *) = {
        [] (list_t *list) {
            if (list->selected_item != 0xFFFF) {
                auto *event_data = FL_MALLOC(vkph::event_data_request_to_join_server_t, 1);
                const void *item_data = list->items[list->selected_item].data;
                const auto *server = (const net::game_server_t *)item_data;

                event_data->server_name = server->server_name;

                // Need to close the main menu, and start a fade effect
                auto *effect_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.0f;
                // In case we need to fade back because of a connection fail
                effect_data->fade_back = 0;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = vkph::ET_ENTER_GAME_PLAY_SCENE;
                effect_data->triggers[0].next_event_data = NULL;
                vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
                vkph::submit_event(vkph::ET_REQUEST_TO_JOIN_SERVER, event_data);
            }
            else if (list->typing_box.is_typing) {
                auto *data = FL_MALLOC(vkph::event_data_request_to_join_server_t, 1);

                memset(data, 0, sizeof(vkph::event_data_request_to_join_server_t));
                data->ip_address = list->typing_box.input_text.get_string();
                // Need to close the main menu, and start a fade effect

                auto *effect_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.0f;
                effect_data->fade_back = 0;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = vkph::ET_ENTER_GAME_PLAY_SCENE;
                effect_data->triggers[0].next_event_data = NULL;
                vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
                vkph::submit_event(vkph::ET_REQUEST_TO_JOIN_SERVER, data);
            }

            list->typing_box.is_typing = 0;
        },

        [] (list_t *list) {
            vkph::submit_event(vkph::ET_REQUEST_REFRESH_SERVER_PAGE, NULL);
            list->typing_box.is_typing = 0;
        }
    };

    servers_list_menu.init(
        &main_menu_layout.current_menu,
        2, button_texts,
        handle_input_procs,
        [] (list_item_t *item) {
            auto *server = (net::game_server_t *)item->data;
            item->text.draw_string(server->server_name, 0xFFFFFFFF);
            item->text.null_terminate();
        });

    refresh_main_menu_server_page();
}

void refresh_build_menu_page(const vkph::state_t *state) {
    const vkph::map_names_t *map_names = &state->map_names;

    build_map_list_menu.clear();
    build_map_list_menu.begin(map_names->count);

    for (uint32_t i = 0; i < map_names->count; ++i) {
        build_map_list_menu.add(&map_names->maps[i]);
    }

    build_map_list_menu.end();
}

static void s_build_map_menu_init(const vkph::state_t *state) {
    const char *button_texts[] = { "Edit" };

    void (* handle_input_procs[1])(list_t *) = {
        [] (list_t *list) {
            if (list->selected_item != 0xFFFF) {
                auto *event_data = FL_MALLOC(vkph::event_enter_map_creator_t, 1);
                // Data contains the path to the map
                const void *item_data = list->items[list->selected_item].data;
                const auto *pair = (const vkph::map_names_t::pair_t *)item_data;

                event_data->map_name = pair->name;
                event_data->map_path = pair->path;

                // Need to close the main menu, and start a fade effect
                auto *effect_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = vkph::ET_ENTER_MAP_CREATOR_SCENE;
                effect_data->triggers[0].next_event_data = event_data;
                vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
            }
            else if (list->typing_box.is_typing) {
                // Code for the "Edit" button
                const char *path = list->typing_box.input_text.get_string();

                auto *data = FL_MALLOC(vkph::event_enter_map_creator_t, 1);
                memset(data, 0, sizeof(vkph::event_enter_map_creator_t));
                data->map_name = NULL;
                data->map_path = list->typing_box.input_text.get_string();

                auto *effect_data = FL_MALLOC(vkph::event_begin_fade_effect_t, 1);
                effect_data->dest_value = 0.0f;
                effect_data->duration = 2.5f;
                effect_data->fade_back = 1;
                effect_data->trigger_count = 1;
                effect_data->triggers[0].trigger_type = vkph::ET_ENTER_MAP_CREATOR_SCENE;
                effect_data->triggers[0].next_event_data = data;
                vkph::submit_event(vkph::ET_BEGIN_FADE, effect_data);
            }
        },
    };

    build_map_list_menu.init(
        &main_menu_layout.current_menu,
        1, button_texts,
        handle_input_procs,
        [] (list_item_t *item) {
            const auto *pair = (vkph::map_names_t::pair_t *)item->data;
            item->text.draw_string(pair->name, 0xFFFFFFFF);
            item->text.null_terminate();
        });

    refresh_build_menu_page(state);
}

static void s_menus_init(const vkph::state_t *state) {
    // Here initialise all the different menus (browse server, build map, settings, etc...)
    s_browse_server_menu_init();
    s_build_map_menu_init(state);
}

static void s_menu_layout_quit_proc() {
    vkph::submit_event(vkph::ET_CLOSED_WINDOW, NULL);
}

void init_main_menu(const vkph::state_t *state) {
    const char *widget_icon_paths[] = { NULL, NULL, NULL, NULL };

    VkDescriptorSet sets[] = {
        get_texture(UT_PLAY_ICON),
        get_texture(UT_BUILD_ICON),
        get_texture(UT_SETTINGS_ICON),
        get_texture(UT_QUIT_ICON),
    };

    menu_click_handler_t procs[] = {
        NULL,
        NULL,
        NULL,
        &s_menu_layout_quit_proc
    };

    main_menu_layout.init(procs, widget_icon_paths, sets, B_INVALID_MENU_BUTTON);

    s_menus_init(state);
}

void submit_main_menu() {
    main_menu_layout.submit();

    if (main_menu_layout.menu_opened()) {
        switch(main_menu_layout.current_open_menu) {
        case B_BROWSE_SERVER: {
            servers_list_menu.submit();
        } break;

        case B_BUILD_MAP: {
            build_map_list_menu.submit();
        } break;

        default: {
        } break;
        }
    }
}
    
void main_menu_input(const app::raw_input_t *input) {
    if (main_menu_layout.input(input)) {
        switch (main_menu_layout.current_open_menu) {
        case B_BROWSE_SERVER: {
            servers_list_menu.input(input);
        } break;

        case B_BUILD_MAP: {
            build_map_list_menu.input(input);
        } break;

        default: {
        } break;
        }
    }
}

void clear_main_menu() {
    servers_list_menu.selected_item = 0xFFFF;
    main_menu_layout.current_button = B_INVALID_MENU_BUTTON;
    main_menu_layout.current_open_menu = main_menu_layout.widget_count;
    main_menu_layout.current_menu.gls_current_size.fx = 0.0f;
    main_menu_layout.close_menus();
    servers_list_menu.typing_box.input_text.reset();
}

}
