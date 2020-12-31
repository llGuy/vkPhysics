#pragma once

namespace srv {

void init_meta_connection();
void check_registration();
void request_registration(const char *server_name);
// Tells meta server that this server is no longer running
void deactivate_server();
void stop_request_thread();

}
