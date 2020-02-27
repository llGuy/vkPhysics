#pragma once

#include <stdint.h>

#include "renderer.hpp"

#include <malloc.h>
#include <intrin.h>

#define FL_MALLOC(type, n) (type *)malloc(sizeof(type) * (n))
#define ALLOCA(type, n) (type *)_malloca(sizeof(type) * (n))

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
