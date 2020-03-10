#include <stdio.h>
#include <string.h>
#include "renderer.hpp"
#include "r_internal.hpp"

#include <ktxvulkan.h>

static attachment_t s_create_cubemap(
    VkExtent3D extent,
    VkFormat format,
    uint32_t mip_levels) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.arrayLayers = 6;
    image_info.extent = extent;
    image_info.format = format;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.mipLevels = mip_levels;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VK_CHECK(vkCreateImage(r_device(), &image_info, NULL, &image));

    VkDeviceMemory memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    image_view_info.format = format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = mip_levels;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 6;

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(r_device(), &image_view_info, NULL, &image_view));

    VkSampler sampler;
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.maxLod = (float)mip_levels;
    
    VK_CHECK(vkCreateSampler(r_device(), &sampler_info, NULL, &sampler));

    attachment_t attachment = {};
    attachment.image = image;
    attachment.image_view = image_view;
    attachment.image_memory = memory;
    attachment.format = format;
    attachment.sampler = sampler;

    return attachment;
}

static VkExtent2D hdr_environment_extent;

static rpipeline_stage_t hdr_environment_init;
static shader_t hdr_environment_init_shader;

VkDescriptorSet r_diffuse_ibl_irradiance() {
    return hdr_environment_init.descriptor_set;
}

static void s_hdr_environment_pass_init() {
    hdr_environment_extent.width = 512;
    hdr_environment_extent.height = 512;
    
    hdr_environment_init.color_attachment_count = 1;
    hdr_environment_init.color_attachments = FL_MALLOC(attachment_t, hdr_environment_init.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = hdr_environment_extent.width;
    extent3d.height = hdr_environment_extent.height;
    extent3d.depth = 1;

    VkExtent2D extent2d = {};
    extent2d.width = 512;
    extent2d.height = 512;

    hdr_environment_init.color_attachments[0] = s_create_cubemap(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    VkAttachmentDescription cubemap_description = r_fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
    VkAttachmentReference cubemap_reference = {};
    cubemap_reference.attachment = 0;
    cubemap_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = hdr_environment_init.color_attachment_count;
    subpass_description.pColorAttachments = &cubemap_reference;
    subpass_description.pDepthStencilAttachment = NULL;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = hdr_environment_init.color_attachment_count;
    render_pass_info.pAttachments = &cubemap_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(r_device(), &render_pass_info, NULL, &hdr_environment_init.render_pass));

    hdr_environment_init.framebuffer = r_create_framebuffer(
        hdr_environment_init.color_attachment_count,
        hdr_environment_init.color_attachments,
        NULL,
        hdr_environment_init.render_pass,
        extent2d,
        6);

    r_rpipeline_descriptor_set_output_init(&hdr_environment_init);

    VkDescriptorType types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    const char *paths[] = {
        "../shaders/SPV/diffuse_ibl.vert.spv",
        "../shaders/SPV/diffuse_ibl.geom.spv",
        "../shaders/SPV/diffuse_ibl.frag.spv"  };
    hdr_environment_init_shader = create_2d_shader(
        NULL,
        0,
        types, 1,
        paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        &hdr_environment_init,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
}

static attachment_t base_cubemap;
static VkDescriptorSet base_cubemap_descriptor_set;

static void s_render_to_diffuse_ibl_cubemap() {
    VkCommandBuffer command_buffer = begin_single_time_command_buffer();

    VkClearValue clear_value = {};
    
    VkRect2D render_area = {};
    render_area.extent = hdr_environment_extent;
    
    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = hdr_environment_init.render_pass;
    begin_info.framebuffer = hdr_environment_init.framebuffer;
    begin_info.renderArea = render_area;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;
    
    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = hdr_environment_extent.width;
    viewport.height = hdr_environment_extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdr_environment_init_shader.pipeline);

    VkDescriptorSet inputs[] = { base_cubemap_descriptor_set };

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        hdr_environment_init_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0, NULL);

    vkCmdDraw(command_buffer, 1, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    
    end_single_time_command_buffer(command_buffer);
}

static shader_t cubemap_shader;

static void s_load_cubemap_texture() {
    // Load with KTX
    ktxTexture *texture;
    
    if (ktxTexture_CreateFromNamedFile("../assets/textures/hdr_environment.ktx", KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) != KTX_SUCCESS) {
        printf("Failed to load cubemap\n");
        exit(1);
    }

    uint32_t width = texture->baseWidth;
    uint32_t height = texture->baseHeight;
    uint32_t layer_count = texture->numLayers;
    uint32_t mip_levels = texture->numLevels;

    ktx_uint8_t *data = ktxTexture_GetData(texture);
    ktx_size_t size = ktxTexture_GetSize(texture);

    gpu_buffer_t staging = create_gpu_buffer(size, data, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VkBufferImageCopy *buffer_copies = FL_MALLOC(VkBufferImageCopy, 6);
    memset(buffer_copies, 0, sizeof(VkBufferImageCopy) * 6);
    
    for (uint32_t face = 0; face < 6; ++face) {
        ktx_size_t offset;
        KTX_error_code result = ktxTexture_GetImageOffset(texture, 0, 0, face, &offset);
        assert(result == KTX_SUCCESS);

        VkBufferImageCopy *buffer_copy_region = &buffer_copies[face];
        buffer_copy_region->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_copy_region->imageSubresource.mipLevel = 0;
        buffer_copy_region->imageSubresource.baseArrayLayer = face;
        buffer_copy_region->imageSubresource.layerCount = 1;
        buffer_copy_region->imageExtent.width = width;
        buffer_copy_region->imageExtent.height = height;
        buffer_copy_region->imageExtent.depth = 1;
        buffer_copy_region->bufferOffset = offset;
    }

    VkExtent3D extent = {};
    extent.width = width;
    extent.height = height;
    extent.depth = 1;
    base_cubemap = s_create_cubemap(extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    VkCommandBuffer command_buffer = begin_single_time_command_buffer();
    
    VkImageMemoryBarrier barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        base_cubemap.image,
        0, 6,
        0, 1,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    vkCmdCopyBufferToImage(
        command_buffer,
        staging.buffer,
        base_cubemap.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        6, buffer_copies);

    barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        base_cubemap.image,
        0, 6,
        0, 1,
        VK_IMAGE_ASPECT_COLOR_BIT);
    
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    end_single_time_command_buffer(command_buffer);

    base_cubemap_descriptor_set = create_image_descriptor_set(
        base_cubemap.image_view,
        base_cubemap.sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

struct cubemap_render_data_t {
    matrix4_t model;
    float invert_y;
};

static void s_cubemap_shader_init() {
    shader_binding_info_t binding_info = {};
    VkDescriptorType descriptor_types[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    const char *shader_paths[] = { "../shaders/SPV/cubemap.vert.spv", "../shaders/SPV/cubemap.frag.spv" };
    
    cubemap_shader = create_3d_shader(
        &binding_info,
        sizeof(cubemap_render_data_t),
        descriptor_types,
        sizeof(descriptor_types) / sizeof(descriptor_types[0]),
        shader_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

void r_environment_init() {
    s_hdr_environment_pass_init();

    s_load_cubemap_texture();

    s_cubemap_shader_init();

    s_render_to_diffuse_ibl_cubemap();
}

void r_render_environment(VkCommandBuffer command_buffer) {
    VkViewport viewport = {};
    viewport.width = r_swapchain_extent().width;
    viewport.height = r_swapchain_extent().height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemap_shader.pipeline);

    VkDescriptorSet descriptor_sets[] = {
        r_camera_transforms_uniform(),
        base_cubemap_descriptor_set };

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        cubemap_shader.layout,
        0,
        2, descriptor_sets,
        0, NULL);

    cpu_camera_data_t *camera = r_cpu_camera_data();
    
    cubemap_render_data_t render_data = {};
    render_data.model = glm::translate(camera->position);
    render_data.invert_y = -1.0f;
    
    vkCmdPushConstants(command_buffer, cubemap_shader.layout, cubemap_shader.flags, 0, sizeof(cubemap_render_data_t), &render_data);

    vkCmdDraw(command_buffer, 36, 1, 0, 0);
}
