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
void r_swapchain_images(uint32_t *image_count, attachment_t *attachments);
VkDevice r_device();
void r_pipeline_init();
void r_execute_final_pass(VkCommandBuffer command_buffer);
VkDescriptorSetLayout r_descriptor_layout(VkDescriptorType type);
VkDescriptorPool r_descriptor_pool();
VkRenderPass r_final_render_pass();
