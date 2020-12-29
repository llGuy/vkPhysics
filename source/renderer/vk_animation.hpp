#pragma once

#include "vk_buffer.hpp"
#include "vk_skeleton.hpp"
#include "vk_descriptor.hpp"

#include <math.hpp>

namespace vk {

constexpr uint32_t MAX_ANIMATION_CYCLES = 10;

struct joint_position_key_frame_t {
    vector3_t position;
    float time_stamp;
};

struct joint_rotation_key_frame_t {
    quaternion_t rotation;
    float time_stamp;
};

struct joint_scale_key_frame_t {
    vector3_t scale;
    float time_stamp;
};

// Index of the joint_key_frames_t in the array = joint id
struct joint_key_frames_t {
    uint32_t position_count;
    joint_position_key_frame_t *positions;

    uint32_t rotation_count;
    joint_rotation_key_frame_t *rotations;

    uint32_t scale_count;
    joint_scale_key_frame_t *scales;
};

struct animation_cycle_t {
    const char *animation_name;
    
    float duration;
    uint32_t joint_animation_count;
    joint_key_frames_t *joint_animations;
};

}
