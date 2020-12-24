#pragma once

#include "vk_scene3d_camera.hpp"
#include "vk_scene3d_lighting.hpp"
#include <vulkan/vulkan.h>

namespace vk {

void prepare_scene3d_lighting();
void prepare_scene3d_camera();
void prepare_scene3d_environment();

struct scene3d_descriptors_t {
    VkDescriptorSet lighting_ubo;
    VkDescriptorSet camera_ubo;
    VkDescriptorSet diff_ibl_tx;
    VkDescriptorSet lut_tx;
    VkDescriptorSet spec_ibl_tx;
};

scene3d_descriptors_t get_scene_descriptors();

}
