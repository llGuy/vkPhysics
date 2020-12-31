#pragma once

#include <net_meta.hpp>

namespace cl {

void init_meta_connection();
void check_registration();
void request_sign_up(const char *username, const char *password);
void request_login(const char *username, const char *password);
void request_available_servers();
void check_meta_request_status_and_handle();
void notify_meta_disconnection();
void stop_request_thread();
void set_path_to_user_meta_info(const char *path);
net::meta_client_t *get_local_meta_client();

}
