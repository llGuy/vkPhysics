#include "vk_skeleton.hpp"

#include <files.hpp>
#include <string.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

namespace vk {

void skeleton_t::load(const char *path) {
    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    joint_count = serialiser.deserialise_uint32();
    joints = flmalloc<joint_t>(joint_count);
    for (uint32_t i = 0; i < joint_count; ++i) {
        joint_t *joint = &joints[i];
        joint->joint_id = serialiser.deserialise_uint32();
        joint->joint_name = create_fl_string(serialiser.deserialise_string());

        for (uint32_t c = 0; c < 4; ++c) {
            joint->inverse_bind_transform[c][0] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][1] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][2] = serialiser.deserialise_float32();
            joint->inverse_bind_transform[c][3] = serialiser.deserialise_float32();
        }

        joint->children_count = serialiser.deserialise_uint32();

        assert(joint->children_count < MAX_CHILD_JOINTS);

        for (uint32_t c = 0; c < joint->children_count; ++c) {
            joint->children_ids[c] = serialiser.deserialise_uint32();
        }
    }
}

}
