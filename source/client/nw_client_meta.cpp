#include <cstddef>
#include <curl/curl.h>
#include <common/log.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include <common/serialiser.hpp>

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
        LOG_INFO("User hasn't signed in yet\n");

        // Request the user to register a name
        submit_event(ET_REQUEST_USER_INFORMATION, NULL, events);
    }
}
