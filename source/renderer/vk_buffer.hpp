#pragma once

#include <vk.hpp>
#include <stdint.h>
#include <vulkan/vulkan.h>

namespace vk {

struct sensitive_gpu_buffer_t {
    uint64_t frame_id;
    gpu_buffer_t buffer;
};

constexpr uint32_t MAX_DELETIONS = 1000;

void init_sensitive_buffer_deletions();

void update_sensitive_buffer_deletions();

}
