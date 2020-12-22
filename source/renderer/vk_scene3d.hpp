#pragma once

#include <vulkan/vulkan.h>

namespace vk {

void prepare_scene3d_data();
// Make sure to update the data that is on the GPU with the new data
// (e.g. new camera transforms, etc...)
void gpu_sync_scene3d_data();

void render_environment(VkCommandBuffer cmdbuf);

struct scene3d_descriptors_t {
    VkDescriptorSet lighting_ubo;
    VkDescriptorSet camera_ubo;
    VkDescriptorSet diff_ibl_tx;
    VkDescriptorSet lut_tx;
    VkDescriptorSet spec_ibl_tx;
};

scene3d_descriptors_t get_scene_descriptors();

}
