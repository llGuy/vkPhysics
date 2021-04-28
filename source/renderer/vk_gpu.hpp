#pragma once

#include <stdint.h>

namespace vk {

struct queue_families_t {
    int32_t graphics_family;
    int32_t present_family;

    inline bool is_complete() {
        return graphics_family >= 0 && present_family >= 0;
    }
};

void init_device();
void destroy_device();

}
