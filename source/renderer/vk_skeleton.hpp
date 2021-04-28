#pragma once

#include <vk.hpp>
#include <math.hpp>

namespace vk {

constexpr uint32_t MAX_CHILD_JOINTS = 5;

struct joint_t {
    uint32_t joint_id = 0;
    // Eventually won't need joint name
    const char *joint_name;
    uint32_t children_count = 0;
    uint32_t children_ids[MAX_CHILD_JOINTS] = {};
    // Inverse of transform going from model space origin to "bind" position of bone
    // "bind" = position of bone by default (without any animations affecting it)
    matrix4_t inverse_bind_transform;
};

}
