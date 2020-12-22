#pragma once

#include "vk_buffer.hpp"
#include "vk_skeleton.hpp"
#include "vk_descriptor.hpp"

#include <common/math.hpp>

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

struct animation_cycles_t {
    uint32_t cycle_count;
    animation_cycle_t *cycles;

    // This also holds some information on how to render this thing
    matrix4_t scale;
    matrix4_t rotation;

    // The linker defines relationship between animations names and IDs
    void load(const char *linker_path, const char *path);
};

struct animated_instance_t {
    float current_animation_time;
    float in_between_interpolation_time = 0.1f;
    bool is_interpolating_between_cycles;

    skeleton_t *skeleton;
    uint32_t prev_bound_cycle;
    uint32_t next_bound_cycle;
    animation_cycles_t *cycles;

    matrix4_t *interpolated_transforms;

    gpu_buffer_t interpolated_transforms_ubo;
    VkDescriptorSet descriptor_set;

    uint32_t *current_position_indices;
    vector3_t *current_positions;
    uint32_t *current_rotation_indices;
    quaternion_t *current_rotations;
    uint32_t *current_scale_indices;
    vector3_t *current_scales;

    void init(skeleton_t *skeleton, animation_cycles_t *cycles);
    void interpolate_joints(float dt, bool repeat);
    void switch_to_cycle(uint32_t cycle_index, bool force);
    void sync_gpu_with_transforms(VkCommandBuffer cmdbuf);
private:
    void calculate_in_between_final_offsets(uint32_t joint_index, matrix4_t parent_transforms);
    void calculate_in_between_bone_space_transforms(float progression, animation_cycle_t *next);
    void calculate_bone_space_transforms(animation_cycle_t *bound_cycle, float current_time);
    void calculate_final_offset(uint32_t joint_index, matrix4_t parent_transform);
};

}
