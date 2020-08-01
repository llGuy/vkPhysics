#pragma once

#include "engine.hpp"

void ui_init(
    struct event_submissions_t *events);

void handle_ui_input(
    highlevel_focus_t focus,
    event_submissions_t *events);

void tick_ui(
    struct event_submissions_t *events);
