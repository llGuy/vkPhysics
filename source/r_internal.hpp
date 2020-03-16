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
    VkCommandBuffer command_buffer);

void r_execute_final_pass(
    VkCommandBuffer command_buffer);

VkDescriptorSetLayout r_descriptor_layout(
    VkDescriptorType type,
    uint32_t count);

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
    rpipeline_stage_t *stage);

VkAttachmentDescription r_fill_color_attachment_description(
    VkImageLayout layout,
    VkFormat format);

VkAttachmentDescription r_fill_depth_attachment_description(
    VkImageLayout layout);

rpipeline_stage_t *r_deferred_stage();

void r_lighting_init();

void r_update_lighting();

struct lighting_data_t {
    vector4_t light_positions[4];
    vector4_t light_colors[4];
    vector4_t vs_directional_light;
};

vector3_t *r_light_direction();

void r_lighting_gpu_sync(VkCommandBuffer command_buffer);

void r_environment_init();

void r_render_environment(VkCommandBuffer command_buffer);
void r_render_environment_to_offscreen();

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
    float fov;
    float near, far;
    matrix4_t previous_view_projection;
};

gpu_camera_transforms_t *r_gpu_camera_data();
cpu_camera_data_t *r_cpu_camera_data();

/* Uniform access */
VkDescriptorSet r_camera_transforms_uniform();
VkDescriptorSet r_lighting_uniform();
