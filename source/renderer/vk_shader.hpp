#pragma once

#include "vk_render_pipeline.hpp"

#include <vk.hpp>
#include <vulkan/vulkan.h>
#include <common/allocators.hpp>

namespace vk {

// Creates the shader modules and fills in VkPipelineShaderStageCreateInfo
VkPipelineShaderStageCreateInfo *fill_shader_stage_create_infos(const char **paths, VkShaderStageFlags flags);
VkPipelineColorBlendStateCreateInfo fill_blend_state_info(
    struct render_pipeline_stage_t *stage,
    alpha_blending_t alpha_blending = AB_NONE);

VkPipelineLayout create_pipeline_layout(
    VkShaderStageFlags shader_flags,
    VkDescriptorType *descriptor_types,
    uint32_t descriptor_layout_count,
    uint32_t push_constant_size);

}
