#pragma once

#include <stdint.h>

#include "renderer.hpp"
#include <common/log.hpp>

#define VK_CHECK(call) if (call != VK_SUCCESS) { LOG_ERRORV("%s failed\n", #call); }

void r_handle_resize(
    uint32_t width,
    uint32_t height);

uint32_t r_image_index();
VkFormat r_depth_format();
VkFormat r_swapchain_format();
VkExtent2D r_swapchain_extent();

VkExtent2D r_shadow_extent();

struct attachment_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkFormat format;
    VkSampler sampler;
};

void r_swapchain_images(
    uint32_t *image_count,
    attachment_t *attachments);

VkDevice r_device();

void r_pipeline_init();

void r_execute_ssao_pass(
    VkCommandBuffer command_buffer);

void r_execute_ssao_blur_pass(
    VkCommandBuffer command_buffer);

void r_execute_lighting_pass(
    VkCommandBuffer command_buffer);

void r_execute_bloom_pass(
    VkCommandBuffer command_buffer);

void r_execute_motion_blur_pass(
    VkCommandBuffer command_buffer,
    VkCommandBuffer ui_command_buffer);

void r_execute_final_pass(
    VkCommandBuffer command_buffer);

VkDescriptorSetLayout r_descriptor_layout(
    VkDescriptorType type,
    uint32_t count);

VkDescriptorPool r_descriptor_pool();

VkRenderPass r_final_render_pass();

void r_camera_handle_input();

void r_camera_init(
    void *window);

void r_camera_gpu_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *info);

VkPipelineShaderStageCreateInfo *r_fill_shader_stage_create_infos(
    const char **paths,
    VkShaderStageFlags flags);

void r_free_shader_stage_create_info(
    VkPipelineShaderStageCreateInfo *info);

VkPipelineLayout r_create_pipeline_layout(
    VkShaderStageFlags shader_flags,
    VkDescriptorType *descriptor_types,
    uint32_t descriptor_layout_count,
    uint32_t push_constant_size);

void r_free_blend_state_info(
    VkPipelineColorBlendStateCreateInfo *info);

/* Rendering pipeline stage (post processing etc..) */
struct rpipeline_stage_t {
    VkRenderPass render_pass;
    
    union {
        VkFramebuffer framebuffer;
        struct {
            // Maximum 3 framebuffers now
            VkFramebuffer framebuffers[3];
        };
    };

    uint32_t color_attachment_count;
    attachment_t *color_attachments;
    
    attachment_t *depth_attachment;

    uint32_t binding_count;
    VkDescriptorSet descriptor_set;
};

void r_free_rpipeline_stage(
    rpipeline_stage_t *stage);

attachment_t r_create_color_attachment(
    VkExtent3D extent, 
    VkFormat format);

attachment_t r_create_depth_attachment(
    VkExtent3D extent);

VkFramebuffer r_create_framebuffer(
    uint32_t color_attachment_count,
    attachment_t *color_attachments, 
    attachment_t *depth_attachment,
    VkRenderPass render_pass,
    VkExtent2D extent,
    uint32_t output_layer_count);

void r_rpipeline_descriptor_set_output_init(
    rpipeline_stage_t *stage);

VkPipelineColorBlendStateCreateInfo r_fill_blend_state_info(
    rpipeline_stage_t *stage,
    alpha_blending_t alpha_blending = AB_NONE);

VkAttachmentDescription r_fill_color_attachment_description(
    VkImageLayout layout,
    VkFormat format);

VkAttachmentDescription r_fill_depth_attachment_description(
    VkImageLayout layout);

rpipeline_stage_t *r_deferred_stage();
rpipeline_stage_t *r_shadow_stage();
rpipeline_stage_t *r_motion_blur_stage();

void r_lighting_init();

void r_update_lighting();

#define MAX_LIGHTS 10

struct lighting_data_t {
    vector4_t light_positions[MAX_LIGHTS];
    vector4_t ws_light_positions[MAX_LIGHTS];
    vector4_t light_directions[MAX_LIGHTS];
    vector4_t ws_light_directions[MAX_LIGHTS];
    vector4_t light_colors[MAX_LIGHTS];
    vector4_t vs_directional_light;
    vector4_t ws_directional_light;

    matrix4_t shadow_view_projection;
    matrix4_t shadow_view;
    matrix4_t shadow_projection;

    vector2_t light_screen_coord;

    int point_light_count;
};

vector3_t *r_light_direction();

void r_lighting_gpu_sync(
    VkCommandBuffer command_buffer,
    lighting_info_t *lighting);

void r_environment_init();
void r_destroy_environment();

void r_render_environment(
    VkCommandBuffer command_buffer);

void r_render_environment_to_offscreen_if_updated(
    VkCommandBuffer command_buffer);

struct base_cubemap_render_data_t {
    matrix4_t inverse_projection;
    
    float width;
    float height;

    float light_direction_x;
    float light_direction_y;
    float light_direction_z;
    float eye_height;
    float rayleigh;
    float mie;
    float intensity;
    float scatter_strength;
    float rayleigh_strength;
    float mie_strength;
    float rayleigh_collection;
    float mie_collection;
    float air_color_r;
    float air_color_g;
    float air_color_b;
};

base_cubemap_render_data_t *r_cubemap_render_data();

VkDescriptorSet r_diffuse_ibl_irradiance();
VkDescriptorSet r_specular_ibl();
VkDescriptorSet r_integral_lookup();

struct gpu_camera_transforms_t {
    matrix4_t projection;
    matrix4_t view;
    matrix4_t inverse_view;
    matrix4_t view_projection;
    vector4_t frustum;
    vector4_t view_direction;
    matrix4_t previous_view_projection;
    float dt;
    float width;
    float height;
};

struct cpu_camera_data_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    vector2_t mouse_position;
    // Degrees
    float fov;
    float near, far;
    matrix4_t previous_view_projection;
};

gpu_camera_transforms_t *r_gpu_camera_data();
cpu_camera_data_t *r_cpu_camera_data();

/* Uniform access */
VkDescriptorSet r_camera_transforms_uniform();
VkDescriptorSet r_lighting_uniform();

void r_environment_debug_menu();

// The stage that is before the stage that renders with the screen as a back buffer
// (As of writing this, it's the motion blur pipeline stage)
rpipeline_stage_t *r_stage_before_final_render();
