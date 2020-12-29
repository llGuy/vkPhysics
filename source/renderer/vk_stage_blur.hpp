#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

#include <math.hpp>

namespace vk {

enum blur_kernel_size_t { BKS_7 = 7, BKS_9 = 9 };

struct blur_stage_t {
    shader_t shader;
    render_pipeline_stage_t stage;
    VkDescriptorSet sets[2];
    VkDescriptorSet current_set;
    VkExtent2D extent;

    void init(VkExtent2D extent, VkFormat format, uint32_t kernel_size);

    void execute(
        VkDescriptorSet input,
        const vector2_t &scale,
        VkCommandBuffer command_buffer);
};

}
