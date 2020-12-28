#pragma once

void nw_init_meta_connection();
void nw_check_registration();
void nw_request_registration(const char *server_name);
// Tells meta server that this server is no longer running
void nw_deactivate_server();
void nw_stop_request_thread();
