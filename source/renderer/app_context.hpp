#pragma once

#include <app.hpp>
#include <stdint.h>
#include <common/math.hpp>

#include "vk_context.hpp"

namespace app {

void api_init();

struct context_info_t {
    void *window;
    const char *name;
    uint32_t width, height;
    vk::surface_proc_t surface_proc;
};

context_info_t init_context(const char *title);

void get_immediate_cursor_pos(double *x, double *y);

}
