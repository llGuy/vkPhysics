#include "vk_sync.hpp"
#include "vk_buffer.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_memory.hpp"
#include "vk_context.hpp"
#include "vk_texture.hpp"

#include <common/log.hpp>
#include <common/files.hpp>

namespace vk {

void texture_t::init(
    const char *path,
    VkFormat t_format,
    void *data,
    uint32_t width, uint32_t height,
    VkFilter filter) {
    format = t_format;
    
    uint32_t pixel_component_size = 1;
    int32_t x, y, channels;
    void *pixels;

    file_handle_t file_handle;
    file_contents_t contents;
    if (!data && path) {
        file_handle = create_file(path, FLF_IMAGE);
        contents = read_file(file_handle);
        pixels = contents.pixels;
        x = contents.width;
        y = contents.height;
        channels = contents.channels;
    }
    else {
        switch (format) {
        case VK_FORMAT_R16G16B16A16_SFLOAT: {
            channels = 4;
            pixel_component_size = 2;
            break;
        }
        default: {
            LOG_ERROR("Need to handle format type for creating texture\n");
            exit(1);
            break;
        }
        }
        
        pixels = data;
        x = width;
        y = height;
    }

    VkExtent3D extent = {};
    extent.width = x;
    extent.height = y;
    extent.depth = 1;
    
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.arrayLayers = 1;
    image_info.extent = extent;
    image_info.format = format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = 1;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(g_ctx->device, &image_info, NULL, &image);

    image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(g_ctx->device, &image_view_info, NULL, &image_view));

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    VK_CHECK(vkCreateSampler(g_ctx->device, &sampler_info, NULL, &sampler));

    descriptor = create_image_descriptor_set(image_view, sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    gpu_buffer_t staging;

    staging.init(
        pixel_component_size * x * y * channels,
        pixels,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer command_buffer = begin_single_use_command_buffer();

    VkImageMemoryBarrier barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        image,
        0,
        1,
        0,
        1,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageExtent.width = x;
    region.imageExtent.height = y;
    region.imageExtent.depth = 1;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    
    vkCmdCopyBufferToImage(
        command_buffer,
        staging.buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        image,
        0,
        1,
        0,
        1,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    end_single_use_command_buffer(command_buffer);

    vkFreeMemory(g_ctx->device, staging.memory, NULL);
    vkDestroyBuffer(g_ctx->device, staging.buffer, NULL);

    if (!data && path) {
        free_image(contents);
        free_file(file_handle);
    }
}

}
