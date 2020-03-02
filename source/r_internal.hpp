#pragma once

#include <stdint.h>

#include "renderer.hpp"

#define FL_MALLOC(type, n) (type *)malloc(sizeof(type) * (n))
#define ALLOCA(type, n) (type *)alloca(sizeof(type) * (n))
#define VK_CHECK(call) if (call != VK_SUCCESS) { printf("%s failed\n", #call); }

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

uint32_t r_image_index();

VkFormat r_depth_format();

VkFormat r_swapchain_format();

VkExtent2D r_swapchain_extent();

void r_swapchain_images(
    uint32_t *image_count,
    attachment_t *attachments);

VkDevice r_device();

void r_pipeline_init();

void r_execute_final_pass(
    VkCommandBuffer command_buffer);

VkDescriptorSetLayout r_descriptor_layout(
    VkDescriptorType type);

VkDescriptorPool r_descriptor_pool();

VkRenderPass r_final_render_pass();

void r_camera_handle_input(
    float dt,
    void *window);

void r_camera_init(
    void *window);

void r_camera_gpu_sync(
    VkCommandBuffer command_buffer);

VkPipelineShaderStageCreateInfo *r_fill_shader_stage_create_infos(
    const char **paths,
    VkShaderStageFlags flags);

void r_free_shader_stage_create_info(
    VkPipelineShaderStageCreateInfo *info);

void r_free_blend_state_info(
    VkPipelineColorBlendStateCreateInfo *info);

/* Rendering pipeline stage (post processing etc..) */
struct rpipeline_stage_t {
    VkRenderPass render_pass;
    VkFramebuffer framebuffer;

    uint32_t color_attachment_count;
    attachment_t *color_attachments;
    
    attachment_t *depth_attachment;
};

VkPipelineColorBlendStateCreateInfo r_fill_blend_state_info(
    rpipeline_stage_t *stage);

rpipeline_stage_t *r_deferred_stage();

VkDescriptorSet r_camera_transforms_uniform();
