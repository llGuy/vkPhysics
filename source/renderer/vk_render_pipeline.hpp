#pragma once

#include <vulkan/vulkan.h>

namespace vk {

enum stage_type_t {
    ST_SHADOW, ST_DEFERRED, ST_POST_PROCESS, ST_UI
};

void fill_main_inheritance_info(VkCommandBufferInheritanceInfo *info, stage_type_t type);

// Structure contains data for a render pipeline stage attachment (for FBO)
struct attachment_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkFormat format;
    VkSampler sampler;

    void init_as_color_attachment(VkExtent3D extent, VkFormat format);
    void init_as_depth_attachment(VkExtent3D extent);

    void init_as_cubemap(
        VkExtent3D extent,
        VkFormat format,
        uint32_t mip_levels,
        VkSamplerAddressMode address_mode);
};

// Creates a framebuffer for a render pipeline stage
VkFramebuffer create_framebuffer(
    uint32_t color_attachment_count,
    attachment_t *color_attachments, 
    attachment_t *depth_attachment,
    VkRenderPass render_pass,
    VkExtent2D extent,
    uint32_t output_layer_count);

struct render_pipeline_stage_t {
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

    void init_descriptor_set_output(bool add_depth = 1);

    // This will just destroy the Vulkan objects
    void destroy();
    // Frees the RAM memory
    void free();
};

// Some utility functions for creating render pipeline stages
VkAttachmentDescription fill_color_attachment_description(VkImageLayout layout, VkFormat format);
VkAttachmentDescription fill_depth_attachment_description(VkImageLayout layout);

struct shadow_stage_t;
struct deferred_stage_t;
struct lighting_stage_t;
struct post_stage_t;
struct ui_stage_t;
struct blur_stage_t;
struct final_stage_t;

// Data that is necessary to carry out a pipeline render
struct render_pipeline_t {
    VkDescriptorSet previous_output;

    // All the render pipeline stages
    shadow_stage_t *shadow;
    deferred_stage_t *deferred;
    lighting_stage_t *lighting;
    post_stage_t *post;
    ui_stage_t *ui;
    // Blur is sometimes used (for instance, in menus)
    blur_stage_t *blur;
    // Conflicts with final keyword (sigh)
    final_stage_t *fin;

    void allocate();
    void init();
    void resize();
    void deallocate();
};

void update_pipeline_previous_output(VkDescriptorSet output);

render_pipeline_stage_t *get_stage_before_final_render();

void post_process_scene(struct frame_info_t *info, VkCommandBuffer ui_cmdbuf);

// This will also resize the swapchain
void resize_render_pipeline(uint32_t width, uint32_t height);

}
