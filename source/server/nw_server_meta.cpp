#include <common/log.hpp>
#include <common/meta.hpp>
#include <common/files.hpp>
#include <common/event.hpp>
#include "common/string.hpp"
#include "nw_server_meta.hpp"
#include <common/serialiser.hpp>
#include <common/allocators.hpp>

static const char *current_server_name;
static uint32_t current_server_id;

void nw_init_meta_connection() {
    begin_meta_client_thread();
}

void nw_check_registration(event_submissions_t *events) {
    file_handle_t file_handle = create_file("assets/.server_meta", FLF_TEXT);
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

        current_server_id = id;

        // Just send to the meta server that this server is active
        LOG_INFOV("Server information found: \"%s\" with ID %d - telling meta server that game server just started\n", server_name, id);

        request_server_active_t *data = FL_MALLOC(request_server_active_t, 1);
        data->server_id = id;
        send_request(R_SERVER_ACTIVE, data);

        event_start_server_t *event_data = FL_MALLOC(event_start_server_t, 1);
        event_data->server_name = server_name;
        submit_event(ET_START_SERVER, event_data, events);
    }
    else {
        bool register_finished = 0;
        LOG_INFO("Server infmroation not found: requesting a name\n");
        LOG_INFO("Please enter a name for this server: ");

        while (!register_finished) {
            char name[50] = {};
            fgets(name, sizeof(char) * 50, stdin);

            for (char *c = name; *c; ++c) {
                if (*c == '\n') *c = 0;
            }

            request_register_server_t *data = FL_MALLOC(request_register_server_t, 1);
            data->server_name = create_fl_string(name);
            current_server_name = data->server_name;

            send_request(R_REGISTER_SERVER, data);

            // Check for server to reply
            char *request_result = NULL;
            uint32_t request_result_size = 0;
            request_t request_type = R_INVALID;
            while (
                !(request_result = check_request_finished(
                      &request_result_size,
                      &request_type)));

            LOG_INFO("Received result\n");

            if (request_result[0] == '1') {
                // Save the server name and the server id
                char *server_id_str = &request_result[2];
                uint32_t server_id = atoi(server_id_str);

                // Save the userid and usertag
                serialiser_t serialiser = {};
                serialiser.init(strlen(current_server_name) + 1 + sizeof(uint32_t) + sizeof(uint32_t));
                serialiser.serialise_string(current_server_name);
                serialiser.serialise_uint32(server_id);

                file_handle_t file = create_file("assets/.server_meta", FLF_OVERWRITE | FLF_WRITEABLE);

                write_file(file, serialiser.data_buffer, serialiser.data_buffer_head);

                free_file(file);

                register_finished = 1;
            }
            else {
                LOG_INFO("Server name not availble, please enter another name for this server: ");
            }
        }

        event_start_server_t *event_data = FL_MALLOC(event_start_server_t, 1);
        event_data->server_name = current_server_name;
        submit_event(ET_START_SERVER, event_data, events);
    }
}

void nw_deactivate_server() {
    request_server_inactive_t *data = FL_MALLOC(request_server_inactive_t, 1);
    data->server_id = current_server_id;
    send_request(R_SERVER_INACTIVE, data);

    join_meta_thread();
}

void nw_stop_request_thread() {
    send_request(R_QUIT, NULL);
    join_meta_thread();
}
