#pragma once

#include "vk_render_pipeline.hpp"

#include <vulkan/vulkan.h>

namespace vk {

enum alpha_blending_t {
    AB_ONE_MINUS_SRC_ALPHA,
    AB_NONE
};

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


struct shader_binding_info_t {
    uint32_t binding_count;
    VkVertexInputBindingDescription *binding_descriptions;

    uint32_t attribute_count;
    VkVertexInputAttributeDescription *attribute_descriptions;
};

struct shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkShaderStageFlags flags;

    // Yes, this takes a lot of parameters
    void init_as_2d_shader(
        shader_binding_info_t *binding_info,
        uint32_t push_constant_size,
        VkDescriptorType *descriptor_layout_types,
        uint32_t descriptor_layout_count,
        const char **shader_paths,
        VkShaderStageFlags shader_flags,
        struct render_pipeline_stage_t *stage,
        VkPrimitiveTopology topology,
        alpha_blending_t alpha_blending);

    void create_as_3d_shader(
        shader_binding_info_t *binding_info,
        uint32_t push_constant_size,
        VkDescriptorType *descriptor_layout_types,
        uint32_t descriptor_layout_count,
        const char **shader_paths,
        VkShaderStageFlags shader_flags,
        struct render_pipeline_stage_t *stage,
        VkPrimitiveTopology topology,
        VkCullModeFlags cull_mode);

    // Some helper functions which just add some more clarity
    // (can use either the functions above, or the ones below)
    // The ones above just give programmer more control
    shader_t create_as_3d_shader_for_stage(
        stage_type_t stage_type,
        shader_binding_info_t *binding_info,
        uint32_t push_constant_size,
        VkDescriptorType *descriptor_layout_types,
        uint32_t descriptor_layout_count,
        const char **shader_paths,
        VkShaderStageFlags shader_flags,
        VkPrimitiveTopology topology,
        VkCullModeFlags culling);
};

}
