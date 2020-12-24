#include "vk_animation.hpp"

#include <common/files.hpp>
#include <common/string.hpp>
#include <common/tokeniser.hpp>
#include <common/serialiser.hpp>
#include <common/containers.hpp>

namespace vk {

// The same linker will be used for al animations
static hash_table_t<uint32_t, 25, 5, 5> animation_name_linker;

static uint32_t s_skip_whitespaces(
    uint32_t i,
    token_t *tokens) {
    for (;; ++i) {
        if (tokens[i].type != TT_WHITESPACE) {
            return i;
        }
    }

    assert(0);
}

struct animation_link_data_t {
    uint32_t animation_count;
    float scale;
    vector3_t rotation;
};

static animation_link_data_t s_fill_animation_linker(
    const char *linker_path) {
    animation_link_data_t data = {};

    data.scale = 1.0f;
    data.rotation = vector3_t(0.0f);

    animation_name_linker.clear();
    animation_name_linker.init();

    uint32_t keyword_ids[] = {
        100,
        101,
        102,
        103,
        104,
        105
    };

    const char *keywords[] = {
        "link",
        "scale",
        "rotate",
        "x",
        "y",
        "z"
    };

    enum {
        KEYWORD_LINK,
        KEYWORD_SCALE,
        KEYWORD_ROTATE,
        X_COMPONENT,
        Y_COMPONENT,
        Z_COMPONENT,
        INVALID_KEYWORD
    };

    tokeniser_init(
        keyword_ids,
        keywords,
        INVALID_KEYWORD);

    file_handle_t file_handle = create_file(linker_path, FLF_NONE);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);
    
    uint32_t token_count = 0;

    token_t *tokens = tokenise(
        (char *)contents.data,
        '#',
        &token_count);

    union {
        struct {
            uint32_t in_comment: 1;
            uint32_t link: 1;
            uint32_t scale: 1;
            uint32_t rotate: 1;
        };

        uint32_t bits;
    } flags = {};

    for (uint32_t i = 0; i < token_count; ++i) {
        if (tokens[i].type == TT_NEW_LINE) {
            flags.in_comment = 0;
        }
        else if (!flags.in_comment) {
            if (tokens[i].type == TT_COMMENT) {
                // Ignore until new line
                flags.in_comment = 1;
            }
            else {
                if (tokens[i].type == keyword_ids[KEYWORD_LINK]) {
                    flags.bits = 0;
                    flags.link = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *dst_id = &tokens[i];
                    uint32_t animation_id = str_to_int(tokens[i].at, tokens[i].length);

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    // Next token should be double quote
                    if (tokens[i].type != TT_DOUBLE_QUOTE) {
                        LOG_ERROR("Error double quote missing\n");
                    }

                    ++i;

                    char *name = tokens[i].at;
                    uint32_t name_length = 0;
                    for (; tokens[i].type != TT_DOUBLE_QUOTE; ++i) {
                        name_length += tokens[i].length;
                    }

                    i = s_skip_whitespaces(i, tokens) - 1; // -1 because of for loop increment

                    animation_name_linker.insert(simple_string_hash(name, name_length), animation_id);

                    ++data.animation_count;
                }
                else if (tokens[i].type == keyword_ids[KEYWORD_SCALE]) {
                    flags.bits = 0;
                    flags.scale = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *scale_value_s = &tokens[i];

                    data.scale = str_to_float(tokens[i].at, tokens[i].length);

                    ++i;

                    i = s_skip_whitespaces(i, tokens);
                }
                else if (tokens[i].type == keyword_ids[KEYWORD_ROTATE]) {
                    flags.bits = 0;
                    flags.rotate = 1;

                    ++i;

                    i = s_skip_whitespaces(i, tokens);

                    token_t *component = &tokens[i];

                    // Get value
                    ++i;
                    i = s_skip_whitespaces(i, tokens);

                    token_t *rotation_value_tk = &tokens[i];
                    float rotation_value = str_to_float(rotation_value_tk->at, rotation_value_tk->length);

                    if (component->type == keyword_ids[X_COMPONENT]) {
                        data.rotation.x = rotation_value;
                    }
                    else if (component->type == keyword_ids[Y_COMPONENT]) {
                        data.rotation.y = rotation_value;
                    }
                    else if (component->type == keyword_ids[Z_COMPONENT]) {
                        data.rotation.z = rotation_value;
                    }

                    ++i;
                    i = s_skip_whitespaces(i, tokens);
                }
            }
        }
    }

    return data;
}

void animation_cycles_t::load(
    const char *linker_path,
    const char *path) {
    animation_link_data_t linked_animations = s_fill_animation_linker(linker_path);

    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    cycle_count = serialiser.deserialise_uint32();
    cycle_count = MAX(linked_animations.animation_count, cycle_count);
    cycles = FL_MALLOC(animation_cycle_t, cycle_count);

    for (uint32_t i = 0; i < cycle_count; ++i) {
        const char *animation_name = create_fl_string(serialiser.deserialise_string());

        uint32_t *animation_id_p = animation_name_linker.get(simple_string_hash(animation_name));
        if (animation_id_p) {
            uint32_t animation_id = *animation_id_p;

            animation_cycle_t *cycle = &cycles[*animation_id_p];
            cycle->animation_name = animation_name;
            cycle->duration = serialiser.deserialise_float32();
            cycle->joint_animation_count = serialiser.deserialise_uint32();
        
            cycle->joint_animations = FL_MALLOC(joint_key_frames_t, cycle->joint_animation_count);

            for (uint32_t j = 0; j < cycle->joint_animation_count; ++j) {
                joint_key_frames_t *key_frames = &cycle->joint_animations[j];
                key_frames->position_count = serialiser.deserialise_uint32();
                key_frames->rotation_count = serialiser.deserialise_uint32();
                key_frames->scale_count = serialiser.deserialise_uint32();

                key_frames->positions = FL_MALLOC(joint_position_key_frame_t, key_frames->position_count);
                key_frames->rotations = FL_MALLOC(joint_rotation_key_frame_t, key_frames->rotation_count);
                key_frames->scales = FL_MALLOC(joint_scale_key_frame_t, key_frames->scale_count);

                for (uint32_t position = 0; position < key_frames->position_count; ++position) {
                    key_frames->positions[position].time_stamp = serialiser.deserialise_float32();
                    key_frames->positions[position].position = serialiser.deserialise_vector3();
                }

                for (uint32_t rotation = 0; rotation < key_frames->rotation_count; ++rotation) {
                    key_frames->rotations[rotation].time_stamp = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.w = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.x = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.y = serialiser.deserialise_float32();
                    key_frames->rotations[rotation].rotation.z = serialiser.deserialise_float32();
                }

                for (uint32_t scale = 0; scale < key_frames->scale_count; ++scale) {
                    key_frames->scales[scale].time_stamp = serialiser.deserialise_float32();
                    key_frames->scales[scale].scale = serialiser.deserialise_vector3();
                }
            }
        }
        else {
            float duration = serialiser.deserialise_float32();
            uint32_t joint_animation_count = serialiser.deserialise_uint32();
        
            for (uint32_t j = 0; j < joint_animation_count; ++j) {
                uint32_t position_count = serialiser.deserialise_uint32();
                uint32_t rotation_count = serialiser.deserialise_uint32();
                uint32_t scale_count = serialiser.deserialise_uint32();

                for (uint32_t position = 0; position < position_count; ++position) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_vector3();
                }

                for (uint32_t rotation = 0; rotation < rotation_count; ++rotation) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                    serialiser.deserialise_float32();
                }

                for (uint32_t scale = 0; scale < scale_count; ++scale) {
                    serialiser.deserialise_float32();
                    serialiser.deserialise_vector3();
                }
            }
        }
    }

    // Create rotation and scale matrices
    matrix4_t rotation_matrix = matrix4_t(1.0f);
    if (linked_animations.rotation.x != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.x), vector3_t(1.0f, 0.0f, 0.0f));
    }
    if (linked_animations.rotation.y != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.y), vector3_t(0.0f, 1.0f, 0.0f));
    }
    if (linked_animations.rotation.z != 0.0f) {
        rotation_matrix  = rotation_matrix * glm::rotate(glm::radians(linked_animations.rotation.z), vector3_t(0.0f, 1.0f, 0.0f));
    }

    matrix4_t scale_matrix = glm::scale(vector3_t(linked_animations.scale));

    scale = scale_matrix;
    rotation = rotation_matrix;
}

void animated_instance_t::init(
    skeleton_t *skeleton,
    animation_cycles_t *acycles) {
    memset(this, 0, sizeof(animated_instance_t));
    current_animation_time = 0.0f;
    is_interpolating_between_cycles = 0;
    this->skeleton = skeleton;
    next_bound_cycle = 0;
    interpolated_transforms = FL_MALLOC(matrix4_t, skeleton->joint_count);
    current_positions = FL_MALLOC(vector3_t, skeleton->joint_count);
    current_rotations = FL_MALLOC(quaternion_t, skeleton->joint_count);
    current_scales = FL_MALLOC(vector3_t, skeleton->joint_count);
    current_position_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    current_rotation_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    current_scale_indices = FL_MALLOC(uint32_t, skeleton->joint_count);
    this->cycles = acycles;
    in_between_interpolation_time = 0.2f;

    memset(current_position_indices, 0, sizeof(uint32_t) * skeleton->joint_count);
    memset(current_rotation_indices, 0, sizeof(uint32_t) * skeleton->joint_count);
    memset(current_scale_indices, 0, sizeof(uint32_t) * skeleton->joint_count);

    for (uint32_t i = 0; i < skeleton->joint_count; ++i) {
        interpolated_transforms[i] = matrix4_t(1.0f);
    }

    interpolated_transforms_ubo.init(
        sizeof(matrix4_t) * skeleton->joint_count,
        interpolated_transforms,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    descriptor_set = create_buffer_descriptor_set(
        interpolated_transforms_ubo.buffer,
        sizeof(matrix4_t) * skeleton->joint_count,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void animated_instance_t::interpolate_joints(float dt, bool repeat) {
    if (is_interpolating_between_cycles) {
        animation_cycle_t *next_cycle = &cycles->cycles[next_bound_cycle];

        if (dt + current_animation_time > in_between_interpolation_time) {
            is_interpolating_between_cycles = 0;

            if (repeat) {
                current_animation_time = 0.0f;
            }
        }
        else {
            float current_time = fmod(current_animation_time + dt, in_between_interpolation_time);
            current_animation_time = current_time;

            float progression = current_animation_time / in_between_interpolation_time;

            calculate_in_between_bone_space_transforms(
                progression,
                next_cycle);

            calculate_in_between_final_offsets(
                0,
                matrix4_t(1.0f));
        }
    }

    // Is not else so that we can continue directly from interpolating between two cycles
    if (!is_interpolating_between_cycles) {
        animation_cycle_t *bound_cycle = &cycles->cycles[next_bound_cycle];

        if (dt + current_animation_time > bound_cycle->duration) {
            if (repeat) {
                for (uint32_t i = 0; i < skeleton->joint_count; ++i) {
                    current_position_indices[i] = 0;
                    current_rotation_indices[i] = 0;
                    current_scale_indices[i] = 0;
                }
            }
            else {
                dt = 0.0f;
            }
        }

        float current_time = fmod(current_animation_time + dt, bound_cycle->duration);
        current_animation_time = current_time;

        calculate_bone_space_transforms(bound_cycle, current_time);
        calculate_final_offset(0, glm::mat4(1.0f));
    }
}

void animated_instance_t::switch_to_cycle(uint32_t cycle_index, bool force) {
    current_animation_time = 0.0f;
    
    if (!force) {
        is_interpolating_between_cycles = 1;
    }

    prev_bound_cycle = next_bound_cycle;
    next_bound_cycle = cycle_index;

    for (uint32_t i = 0; i < skeleton->joint_count; ++i) {
        current_position_indices[i] = 0;
        current_rotation_indices[i] = 0;
        current_scale_indices[i] = 0;
    }
}

void animated_instance_t::sync_gpu_with_transforms(VkCommandBuffer cmdbuf) {
    interpolated_transforms_ubo.update(
        cmdbuf,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(matrix4_t) * skeleton->joint_count,
        interpolated_transforms);
}

void animated_instance_t::calculate_in_between_final_offsets(
    uint32_t joint_index,
    matrix4_t parent_transforms) {
    matrix4_t local_transform = interpolated_transforms[joint_index];

    matrix4_t model_transform = parent_transforms * local_transform;

    joint_t *joint_data = &skeleton->joints[joint_index];
    for (uint32_t child = 0; child < joint_data->children_count; ++child) {
        calculate_in_between_final_offsets(joint_data->children_ids[child], model_transform);
    }

    interpolated_transforms[joint_index] = model_transform * joint_data->inverse_bind_transform;
}

void animated_instance_t::calculate_in_between_bone_space_transforms(
    float progression,
    animation_cycle_t *next) {
    for (uint32_t joint_index = 0; joint_index < skeleton->joint_count; ++joint_index) {
        vector3_t *current_position = &current_positions[joint_index];
        quaternion_t *current_rotation = &current_rotations[joint_index];
        vector3_t *current_scale = &current_scales[joint_index];

        joint_key_frames_t *next_key_frames = &next->joint_animations[joint_index];

        joint_position_key_frame_t *next_position_kf = &next_key_frames->positions[0];
        joint_rotation_key_frame_t *next_rotation_kf = &next_key_frames->rotations[0];

        vector3_t next_position = next_position_kf->position;
        quaternion_t next_rotation = next_rotation_kf->rotation;

        vector3_t new_position = *current_position + progression * (next_position - *current_position);
        quaternion_t new_rotation = glm::slerp(*current_rotation, next_rotation, progression);

        // For now don't interpolate scale
        *current_scale = vector3_t(1.0f);

        // For now, store here
        interpolated_transforms[joint_index] = glm::translate(new_position) * glm::mat4_cast(new_rotation) * glm::scale(*current_scale);
    }
}

void animated_instance_t::calculate_bone_space_transforms(
    animation_cycle_t *bound_cycle,
    float current_time) {
    for (uint32_t joint_index = 0; joint_index < skeleton->joint_count; ++joint_index) {
        vector3_t *current_position = &current_positions[joint_index];
        quaternion_t *current_rotation = &current_rotations[joint_index];
        vector3_t *current_scale = &current_scales[joint_index];

        joint_key_frames_t *current_joint_keyframes = &bound_cycle->joint_animations[joint_index];

        for (
            uint32_t p = current_position_indices[joint_index];
            p < current_joint_keyframes->position_count;
            ++p) {
            if (current_joint_keyframes->positions[p].time_stamp > current_time) {
                uint32_t next_position_index = p;
                uint32_t prev_position_index = p - 1;

                joint_position_key_frame_t *prev = &current_joint_keyframes->positions[prev_position_index];
                joint_position_key_frame_t *next = &current_joint_keyframes->positions[next_position_index];
                float position_progression = (current_time - prev->time_stamp) / (next->time_stamp - prev->time_stamp);
                *current_position = prev->position + position_progression * (next->position - prev->position);

                current_position_indices[joint_index] = prev_position_index;

                break;
            }
        }

        for (
            uint32_t r = current_rotation_indices[joint_index];
            r < current_joint_keyframes->rotation_count;
            ++r) {
            if (current_joint_keyframes->rotations[r].time_stamp > current_time) {
                uint32_t next_rotation_index = r;
                uint32_t prev_rotation_index = r - 1;

                joint_rotation_key_frame_t *prev = &current_joint_keyframes->rotations[prev_rotation_index];
                joint_rotation_key_frame_t *next = &current_joint_keyframes->rotations[next_rotation_index];
                float rotation_progression = (current_time - prev->time_stamp) / (next->time_stamp - prev->time_stamp);
                *current_rotation = glm::slerp(prev->rotation, next->rotation, rotation_progression);

                current_rotation_indices[joint_index] = prev_rotation_index;

                break;
            }
        }

        // For now don't interpolate scale
        *current_scale = vector3_t(1.0f);
    }
}

void animated_instance_t::calculate_final_offset(
    uint32_t joint_index,
    matrix4_t parent_transform) {
    vector3_t *current_position = &current_positions[joint_index];
    quaternion_t *current_rotation = &current_rotations[joint_index];
    vector3_t *current_scale = &current_scales[joint_index];

    matrix4_t local_transform = glm::translate(*current_position) * glm::mat4_cast(*current_rotation) * glm::scale(*current_scale);

    matrix4_t model_transform = parent_transform * local_transform;

    joint_t *joint_data = &skeleton->joints[joint_index];
    for (uint32_t child = 0; child < joint_data->children_count; ++child) {
        calculate_final_offset(joint_data->children_ids[child], model_transform);
    }

    interpolated_transforms[joint_index] = model_transform * joint_data->inverse_bind_transform;
}

}
