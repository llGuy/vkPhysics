#include <cstddef>
#include <cstdlib>
#include <stdio.h>
#include <sha1.hpp>
#include <string.h>
#include "nw_client.hpp"
#include <log.hpp>
#include <files.hpp>
#include <vkph_events.hpp>
#include <string.hpp>
#include <net_context.hpp>
#include "nw_client_meta.hpp"
#include <vkph_event_data.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

#include <net_meta.hpp>

static net::meta_client_t current_client;
// Default path is to "assets/.user_meta", although, for debugging,
// it is often useful to have other users
static const char *path_to_user_meta_info = "assets/.user_meta";

void nw_init_meta_connection() {
    net::begin_meta_client_thread();

    current_client.username = NULL;
}

void nw_set_path_to_user_meta_info(const char *path) {
    path_to_user_meta_info = path;
}

void nw_check_registration() {
    LOG_INFOV("Reading user information from path: %s\n", path_to_user_meta_info);

    file_handle_t file_handle = create_file(path_to_user_meta_info, FLF_TEXT);
    file_contents_t contents = read_file(file_handle);

    // .user_meta should contain user id, user tag and user name
    // the user id can not be 0 - if it is, we haven't registered yet

    static const char *invalid_username = "NO_USER";

    bool registered_username = 0;

    for (uint32_t i = 0; i < strlen(invalid_username); ++i) {
        // If there is a difference, then the user has registered
        if (invalid_username[i] != contents.data[i]) {
            registered_username = 1;
        }
    }

    if (registered_username) {
        // Deserialise stuff
        serialiser_t serialiser {};
        serialiser.data_buffer = (uint8_t *)contents.data;
        serialiser.data_buffer_size = contents.size;

        const char *username = serialiser.deserialise_string();
        current_client.username = username;
        uint32_t usertag = serialiser.deserialise_uint32();
        uint32_t uid = serialiser.deserialise_uint32();

        // Automatically log into the server
        LOG_INFOV("User information found: \"%s\" with tag %d and user ID %d - attempting to login in...\n", username, usertag, uid);
        net::request_automatic_login_t *login_data = FL_MALLOC(net::request_automatic_login_t, 1);
        login_data->userid = uid;
        login_data->usertag = usertag;

        send_request(net::R_AUTOMATIC_LOGIN, login_data);

        auto *start_client_data = FL_MALLOC(vkph::event_start_client_t, 1);
        start_client_data->client_name = current_client.username;
        vkph::submit_event(vkph::ET_START_CLIENT, start_client_data);
    }
    else {
        LOG_INFO("No user information has been found in /assets/.user_meta - requesting user to sign up or login...\n");

        // Request the user to register a name
        vkph::submit_event(vkph::ET_REQUEST_USER_INFORMATION, NULL);
    }
}

void nw_check_meta_request_status_and_handle() {
    uint32_t size = 0;
    net::request_t request_type;
    char *data = check_request_finished(&size, &request_type);

    if (data) {
        // Request was finished
        switch (request_type) {
        case net::R_SIGN_UP: case net::R_LOGIN: {
            if (data[0] == '1') {
                // Exit sign up menu
                vkph::submit_event(vkph::ET_SIGN_UP_SUCCESS, NULL);

                // The 2 bytes corresond to the 1 and the new line
                char *user_tag_str = data + 2;
                uint32_t usertag = atoi(user_tag_str);

                char *user_id_str = skip_to(user_tag_str, '\n');
                uint32_t userid = atoi(user_id_str);

                // Save the userid and usertag
                serialiser_t serialiser = {};
                serialiser.init(strlen(current_client.username) + 1 + sizeof(uint32_t) + sizeof(uint32_t));
                serialiser.serialise_string(current_client.username);
                serialiser.serialise_uint32(usertag);
                serialiser.serialise_uint32(userid);

                file_handle_t file = create_file(path_to_user_meta_info, FLF_OVERWRITE | FLF_WRITEABLE);

                write_file(file, serialiser.data_buffer, serialiser.data_buffer_head);

                free_file(file);

                // Request available servers list
                nw_request_available_servers();

                auto *start_client_data = FL_MALLOC(vkph::event_start_client_t, 1);
                start_client_data->client_name = current_client.username;
                vkph::submit_event(vkph::ET_START_CLIENT, start_client_data);
            }
            else {
                // Failed to sign up or login
                auto *data = FL_MALLOC(vkph::event_meta_request_error_t, 1);

                if (request_type == net::R_SIGN_UP) {
                    data->error_type = net::RE_USERNAME_EXISTS;
                }
                else {
                    data->error_type = net::RE_INCORRECT_PASSWORD_OR_USERNAME;
                }

                vkph::submit_event(vkph::ET_META_REQUEST_ERROR, data);
            }
        } break;

        case net::R_AUTOMATIC_LOGIN: {
            if (data[0] == '1') {
                LOG_INFO("Succeeded to login - game may begin\n");

                // Request meta server - which servers are online at the moment
                net::request_available_server_t *data = FL_MALLOC(net::request_available_server_t, 1);
                send_request(net::R_AVAILABLE_SERVERS, data);
            }
            else {
                LOG_INFO("Failed to login\n");

                // Request the user to login (or sign up) a name
                vkph::submit_event(vkph::ET_REQUEST_USER_INFORMATION, NULL);
            }
        } break;

        case net::R_AVAILABLE_SERVERS: {
            nw_get_available_servers()->name_to_server.clear();

            uint32_t server_count = atoi(data);

            LOG_INFOV("There are %d servers\n", server_count);

            char *server_str = &data[2];

            uint32_t i = 0;

            bool processing = 1;
            while (processing) {
                if (*server_str == '\0' || *server_str == '\n') {
                    processing = 0;
                }
                else {
                    char *server_id_start = server_str;
                    server_str = skip_to(server_str, ';');
                    uint32_t server_id_length = (server_str - server_id_start) - 1;

                    char *server_name_start = server_str;
                    server_str = skip_to(server_str, ';');
                    uint32_t server_name_length = (server_str - server_name_start) - 1;

                    char *ip_start = server_str;
                    server_str = skip_to(server_str, ';');
                    uint32_t ip_length = (server_str - ip_start) - 1;

                    char *player_count_start = server_str;
                    server_str = skip_to(server_str, '\n');
                    uint32_t player_count_length = (server_str - player_count_start) - 1;

                    uint32_t server_id = atoi(server_id_start);
                    const char *server_name = create_fl_string(server_name_start, server_name_length);
                    const char *ip_address = create_fl_string(ip_start, ip_length);
                    uint32_t player_count = atoi(player_count_start);

                    LOG_INFOV("Server (%d) called \"%s\" (ip: %s) has %d player(s) active\n", server_id, server_name, ip_address, player_count);

                    net::game_server_t *dst = &nw_get_available_servers()->servers[i];
                    dst->server_name = server_name;
                    dst->ipv4_address = str_to_ipv4_int32(ip_address, net::GAME_OUTPUT_PORT_SERVER, net::SP_UDP);
                    nw_get_available_servers()->name_to_server.insert(simple_string_hash(server_name), i);

                    ++i;
                }
            }

            nw_get_available_servers()->server_count = i;

            vkph::submit_event(vkph::ET_RECEIVED_AVAILABLE_SERVERS, NULL);
        } break;
        }
    }
}

void nw_request_sign_up(
    const char *username,
    const char *password) {
    net::request_sign_up_data_t *sign_up_data = FL_MALLOC(net::request_sign_up_data_t, 1);
    current_client.username = username;

    sign_up_data->username = username;
    sign_up_data->password = password;

    // Sends request to the web server
    send_request(net::R_SIGN_UP, sign_up_data);
}

void nw_request_login(
    const char *username,
    const char *password) {
    net::request_login_data_t *login_data = FL_MALLOC(net::request_login_data_t, 1);
    current_client.username = username;

    login_data->username = username;
    login_data->password = password;

    // Sends request to the web server
    send_request(net::R_LOGIN, login_data);
}

void nw_request_available_servers() {
    LOG_INFO("Requesting available servers from meta server\n");

    // Request meta server - which servers are online at the moment
    net::request_available_server_t *data = FL_MALLOC(net::request_available_server_t, 1);
    send_request(net::R_AVAILABLE_SERVERS, data);
}

void nw_notify_meta_disconnection() {
    send_request(net::R_QUIT, NULL);
    LOG_INFO("Telling meta server we disconnected\n");
}

void nw_stop_request_thread() {
    net::join_meta_thread();
}

net::meta_client_t *nw_get_local_meta_client() {
    return &current_client;
}
