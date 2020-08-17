#pragma once

void nw_init_meta_connection();
void nw_check_registration();
void nw_request_sign_up(const char *username, const char *password);
void nw_check_meta_request_status_and_handle(struct event_submissions_t *events);
void nw_stop_request_thread();
