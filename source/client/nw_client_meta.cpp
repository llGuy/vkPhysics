#include <cstddef>
#include <stdio.h>
#include <sha1.hpp>
#include <string.h>
#include <common/log.hpp>
#include <common/meta.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include <common/serialiser.hpp>
#include <common/allocators.hpp>

static const char *current_username;

void nw_init_meta_connection() {
    begin_meta_client_thread();
}

void nw_check_registration(event_submissions_t *events) {
    file_handle_t file_handle = create_file("assets/.user_meta", FLF_TEXT);
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
        uint32_t usertag = serialiser.deserialise_uint32();
        uint32_t uid = serialiser.deserialise_uint32();

        // Automatically log into the server
        LOG_INFOV("User information found: \"%s\" with tag %d and user ID %d - attempting to login in...\n", username, usertag, uid);
        request_automatic_login_t *login_data = FL_MALLOC(request_automatic_login_t, 1);
        login_data->userid = uid;
        login_data->usertag = usertag;

        send_request(R_AUTOMATIC_LOGIN, login_data);
    }
    else {
        LOG_INFO("No user information has been found in /assets/.user_meta - requesting user to sign up or login...\n");

        // Request the user to register a name
        submit_event(ET_REQUEST_USER_INFORMATION, NULL, events);
    }
}

static char *s_skip_line(char *pointer) {
    while (*pointer) {
        switch (*pointer) {
        case '\n': return ++pointer;
        default: ++pointer;
        }
    }

    return NULL;
}

void nw_check_meta_request_status_and_handle(event_submissions_t *events) {
    uint32_t size = 0;
    request_t request_type;
    char *data = check_request_finished(&size, &request_type);

    if (data) {
        // Request was finished
        switch (request_type) {
        case R_SIGN_UP: case R_LOGIN: {
            if (data[0] == '1') {
                // Exit sign up menu
                submit_event(ET_SIGN_UP_SUCCESS, NULL, events);

                // The 2 bytes corresond to the 1 and the new line
                char *user_tag_str = data + 2;
                uint32_t usertag = atoi(user_tag_str);

                char *user_id_str = s_skip_line(user_tag_str);
                uint32_t userid = atoi(user_id_str);

                // Save the userid and usertag
                serialiser_t serialiser = {};
                serialiser.init(strlen(current_username) + 1 + sizeof(uint32_t) + sizeof(uint32_t));
                serialiser.serialise_string(current_username);
                serialiser.serialise_uint32(usertag);
                serialiser.serialise_uint32(userid);

                file_handle_t file = create_file("assets/.user_meta", FLF_OVERWRITE | FLF_WRITEABLE);

                write_file(file, serialiser.data_buffer, serialiser.data_buffer_head);

                free_file(file);
            }
            else {
                // Failed to sign up or login
                event_meta_request_error_t *data = FL_MALLOC(event_meta_request_error_t, 1);

                if (request_type == R_SIGN_UP) {
                    data->error_type = RE_USERNAME_EXISTS;
                }
                else {
                    data->error_type = RE_INCORRECT_PASSWORD_OR_USERNAME;
                }

                submit_event(ET_META_REQUEST_ERROR, data, events);
            }
        } break;

        case R_AUTOMATIC_LOGIN: {
            if (data[0] == '1') {
                LOG_INFO("Succeeded to login - game may begin\n");
            }
            else {
                LOG_INFO("Failed to login\n");

                // Request the user to login (or sign up) a name
                submit_event(ET_REQUEST_USER_INFORMATION, NULL, events);
            }
        } break;
        }
    }
}

void nw_request_sign_up(
    const char *username,
    const char *password) {
    request_sign_up_data_t *sign_up_data = LN_MALLOC(request_sign_up_data_t, 1);
    current_username = username;

    sign_up_data->username = username;
    sign_up_data->password = password;

    // Sends request to the web server
    send_request(R_SIGN_UP, sign_up_data);
}

void nw_request_login(
    const char *username,
    const char *password) {
    request_login_data_t *login_data = LN_MALLOC(request_login_data_t, 1);
    current_username = username;

    login_data->username = username;
    login_data->password = password;

    // Sends request to the web server
    send_request(R_LOGIN, login_data);
}

void nw_stop_request_thread() {
    send_request(R_QUIT, NULL);
    join_meta_thread();
}
