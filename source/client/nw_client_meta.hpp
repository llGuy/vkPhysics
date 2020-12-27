#pragma once

void nw_init_meta_connection();
void nw_check_registration();
void nw_request_sign_up(const char *username, const char *password);
void nw_request_login(const char *username, const char *password);
void nw_request_available_servers();
void nw_check_meta_request_status_and_handle();
void nw_notify_meta_disconnection();
void nw_stop_request_thread();
void nw_set_path_to_user_meta_info(const char *path);
