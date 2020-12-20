#pragma once

#include <vulkan/vulkan.h>

namespace vk {

struct frame_t {
    VkCommandBuffer cmdbuf;
};

frame_t begin_frame();

struct frame_info_t {
    union {
        struct {
            uint32_t blurred: 1;
            uint32_t ssao: 1;
            uint32_t debug_window: 1;
        };

        uint32_t flags;
    };
};

void end_frame(const frame_info_t *info);

}
