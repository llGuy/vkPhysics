#include <common/log.hpp>
#include <common/meta.hpp>
#include <common/files.hpp>
#include "nw_server_meta.hpp"
#include <common/serialiser.hpp>

static const char *current_server_name;

void nw_init_meta_connection() {
    begin_meta_client_thread();
}

void nw_check_registration() {
    file_handle_t file_handle = create_file("assets/.user_meta", FLF_TEXT);
    file_contents_t contents = read_file(file_handle);

    // .user_meta should contain user id, user tag and user name
    // the user id can not be 0 - if it is, we haven't registered yet

    static const char *invalid_username = "NO_SERVER";

    bool registered_server_name = 0;

    for (uint32_t i = 0; i < strlen(invalid_username); ++i) {
        // If there is a difference, then the user has registered
        if (invalid_username[i] != contents.data[i]) {
            registered_server_name = 1;
        }
    }

    if (registered_server_name) {
        serialiser_t serialiser = {};
        serialiser.data_buffer = (uint8_t *)contents.data;
        serialiser.data_buffer_size = contents.size;

        const char *server_name = serialiser.deserialise_string();
        uint32_t id = serialiser.deserialise_uint32();

        // Just send to the meta server that this server is active
        LOG_INFOV("Server information found: \"%s\" with ID %d - telling meta server that game server just started\n", server_name, id);
    }
}
