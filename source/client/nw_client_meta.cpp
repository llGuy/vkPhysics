#include <cstddef>
#include <stdio.h>
#include <sha1.hpp>
#include <common/log.hpp>
#include <common/meta.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include <common/serialiser.hpp>
#include <common/allocators.hpp>

void nw_init_meta_connection() {
    // begin_meta_client_thread();
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
        uint32_t uid = serialiser.deserialise_uint32();
        uint32_t usertag = serialiser.deserialise_uint32();

        // Automatically log into the server
        LOG_INFO("User already signed in\n");
    }
    else {
        LOG_INFO("User hasn't signed up yet\n");

        // Request the user to register a name
        submit_event(ET_REQUEST_USER_INFORMATION, NULL, events);
    }
}

void nw_check_meta_request_status_and_handle(event_submissions_t *events) {
#if 0
    uint32_t size = 0;
    request_t request_type;
    char *data = check_request_finished(&size, &request_type);

    if (data) {
        // Request was finished
        switch (request_type) {
        case R_SIGN_UP: {
            if (data[0] == '0') {
                // Failed to sign up - username was taken
                event_meta_request_error_t *data = FL_MALLOC(event_meta_request_error_t, 1);
                data->error_type = RE_USERNAME_EXISTS;
                submit_event(ET_META_REQUEST_ERROR, data, events);
            }
            else {
                // Save the userid and usertag

#if 0
                submit_event(ET_SIGN_UP_SUCCESS, NULL, events);
#endif
            }
        } break;
        }
    }
#endif
}

void nw_request_sign_up(
    const char *username,
    const char *password) {
#if 0
    request_sign_up_data_t *sign_up_data = LN_MALLOC(request_sign_up_data_t, 1);
    sign_up_data->username = username;
    sign_up_data->password = password;

    // Sends request to the web server
    send_request(R_SIGN_UP, sign_up_data);
#endif
}

void nw_stop_request_thread() {
#if 0
    send_request(R_QUIT, NULL);
    join_meta_thread();
#endif
}
