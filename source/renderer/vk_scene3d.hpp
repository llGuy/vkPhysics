#pragma once

#include "vk_scene3d_camera.hpp"
#include "vk_scene3d_lighting.hpp"
#include <vulkan/vulkan.h>

namespace vk {

void prepare_scene3d_data();
// Make sure to update the data that is on the GPU with the new data
// (e.g. new camera transforms, etc...)
void gpu_sync_scene3d_data(VkCommandBuffer cmdbuf, eye_3d_info_t *eye, lighting_info_t *light);

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
