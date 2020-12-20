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

    void init_descriptor_set_output(bool add_depth);
};

// Some utility functions for creating render pipeline stages
VkAttachmentDescription fill_color_attachment_description(VkImageLayout layout, VkFormat format);
VkAttachmentDescription fill_depth_attachment_description(VkImageLayout layout);

}
