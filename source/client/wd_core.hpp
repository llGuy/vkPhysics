#pragma once

#include "cl_view.hpp"

// World initialise
void wd_init(struct event_submissions_t *events);
void wd_destroy();
void wd_game_input(float dt);
void wd_tick(event_submissions_t *events);
void wd_set_i_am_in_server(bool b);
bool wd_am_i_in_server();
void wd_clear_world();
