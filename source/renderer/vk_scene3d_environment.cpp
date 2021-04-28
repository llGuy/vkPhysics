#include "vk.hpp"
#include "vk_sync.hpp"
#include "vk_memory.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_context.hpp"
#include "vk_scene3d_environment.hpp"

namespace vk {

void integral_lut_t::init() {
    init_render_pass();
    render_to_lut();

    set = init_.descriptor_set;
    img = init_.color_attachments[0];

    cleanup_resources();
};

void integral_lut_t::init_render_pass() {
    init_.color_attachment_count = 1;
    init_.color_attachments = flmalloc<attachment_t>(init_.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = 512;
    extent3d.height = 512;
    extent3d.depth = 1;

    VkExtent2D extent2d = {};
    extent2d.width = 512;
    extent2d.height = 512;

    init_.color_attachments[0].init_as_color_attachment(extent3d, VK_FORMAT_R16G16_SFLOAT);

    VkAttachmentDescription cubemap_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16_SFLOAT);
    
    VkAttachmentReference cubemap_reference = {};
    cubemap_reference.attachment = 0;
    cubemap_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = init_.color_attachment_count;
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
    render_pass_info.attachmentCount = init_.color_attachment_count;
    render_pass_info.pAttachments = &cubemap_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &init_.render_pass));

    init_.framebuffer = create_framebuffer(
        init_.color_attachment_count,
        init_.color_attachments,
        NULL,
        init_.render_pass,
        extent2d,
        1);

    init_.init_descriptor_set_output();

    const char *paths[] = {
        "shaders/SPV/integral_lookup.vert.spv",
        "shaders/SPV/integral_lookup.frag.spv"
    };

    shader_create_info_t info;
    info.shader_paths = paths;
    info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    init_shader_.init_as_2d_shader(&info, &init_);
}

void integral_lut_t::render_to_lut() {
    VkCommandBuffer command_buffer = begin_single_use_command_buffer();

    VkClearValue clear_value = {};

    VkRect2D render_area = {};
    render_area.extent.width = 512;
    render_area.extent.height = 512;

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = init_.render_pass;
    begin_info.framebuffer = init_.framebuffer;
    begin_info.renderArea = render_area;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = 512;
    viewport.height = 512;
    viewport.maxDepth = 1;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent.width = 512;
    rect.extent.height = 512;
    vkCmdSetScissor(command_buffer, 0, 1, &rect);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, init_shader_.pipeline);

    vkCmdDraw(command_buffer, 4, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    end_single_use_command_buffer(command_buffer);
}

void integral_lut_t::cleanup_resources() {
    vkDestroyFramebuffer(g_ctx->device, init_.framebuffer, NULL);
    vkDestroyRenderPass(g_ctx->device, init_.render_pass, NULL);

    init_.free();
}

void atmosphere_model_default_values(atmosphere_model_t *model, const vector3_t &directional_light) {
    float light_direction[3] = { directional_light.x, directional_light.y, directional_light.z };
    float rayleigh = -0.082f;
    float mie = -0.908f;
    float intensity = 0.650f;
    float scatter_strength = 1.975f;
    float rayleigh_strength = 2.496f;
    float mie_strength = 0.034f;
    float rayleigh_collection = 8.0f;
    float mie_collection = 2.981f;
    float kr[3] = {27.0f / 255.0f, 82.0f / 255.0f, 111.0f / 255.0f};
    
    model->light_direction_x = light_direction[0];
    model->light_direction_y = light_direction[1];
    model->light_direction_z = light_direction[2];
    model->rayleigh = rayleigh;
    model->mie = mie;
    model->intensity = intensity;
    model->scatter_strength = scatter_strength;
    model->rayleigh_strength = rayleigh_strength;
    model->mie_strength = mie_strength;
    model->rayleigh_collection = rayleigh_collection;
    model->mie_collection = mie_collection;
    model->air_color_r = kr[0];
    model->air_color_g = kr[1];
    model->air_color_b = kr[2];
}

void pbr_environment_t::prepare_diff_ibl() {
    extent.width = 512;
    extent.height = 512;
    
    diff_ibl_init.color_attachment_count = 1;
    diff_ibl_init.color_attachments = flmalloc<attachment_t>(diff_ibl_init.color_attachment_count);

    VkExtent3D extent3d = {};
    extent3d.width = extent.width;
    extent3d.height = extent.height;
    extent3d.depth = 1;

    VkExtent2D extent2d = {};
    extent2d.width = 512;
    extent2d.height = 512;

    diff_ibl_init.color_attachments[0].init_as_cubemap(
        extent3d,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        1,
        VK_SAMPLER_ADDRESS_MODE_REPEAT);

    VkAttachmentDescription cubemap_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
    VkAttachmentReference cubemap_reference = {};
    cubemap_reference.attachment = 0;
    cubemap_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = diff_ibl_init.color_attachment_count;
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
    render_pass_info.attachmentCount = diff_ibl_init.color_attachment_count;
    render_pass_info.pAttachments = &cubemap_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &diff_ibl_init.render_pass));

    diff_ibl_init.framebuffer = create_framebuffer(
        diff_ibl_init.color_attachment_count,
        diff_ibl_init.color_attachments,
        NULL,
        diff_ibl_init.render_pass,
        extent2d,
        6);

    diff_ibl_init.init_descriptor_set_output();

    VkDescriptorType types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };

    const char *paths[] = {
        "shaders/SPV/diffuse_ibl.vert.spv",
        "shaders/SPV/diffuse_ibl.geom.spv",
        "shaders/SPV/diffuse_ibl.frag.spv"
    };

    shader_create_info_t info;
    info.descriptor_layout_types = types;
    info.descriptor_layout_count = 1;
    info.shader_paths = paths;
    info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    diff_ibl_init_shader.init_as_2d_shader(&info, &diff_ibl_init);
}

void pbr_environment_t::render_to_diff_ibl(
    VkDescriptorSet base_cubemap,
    VkCommandBuffer cmdbuf) {
    VkClearValue clear_value = {};
    
    VkRect2D render_area = {};
    render_area.extent = extent;
    
    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = diff_ibl_init.render_pass;
    begin_info.framebuffer = diff_ibl_init.framebuffer;
    begin_info.renderArea = render_area;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;
    
    vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent.width = extent.width;
    rect.extent.height = extent.height;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, diff_ibl_init_shader.pipeline);

    VkDescriptorSet inputs[] = { base_cubemap };

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        diff_ibl_init_shader.layout,
        0,
        sizeof(inputs) / sizeof(VkDescriptorSet),
        inputs,
        0, NULL);

    vkCmdDraw(cmdbuf, 1, 1, 0, 0);

    vkCmdEndRenderPass(cmdbuf);
}

struct spec_ibl_render_data_t {
    matrix4_t face_rotation;
    float roughness;
    float layer;
};

void pbr_environment_t::prepare_spec_ibl() {
    VkExtent3D extent3d = {};
    extent3d.width = 512;
    extent3d.height = 512;
    extent3d.depth = 1;
    
    spec_ibl_cubemap.init_as_cubemap(
        extent3d,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        5,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    spec_ibl_set = create_image_descriptor_set(
        spec_ibl_cubemap.image_view,
        spec_ibl_cubemap.sampler,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    
    spec_ibl_init.color_attachment_count = 1;
    spec_ibl_init.color_attachments = flmalloc<attachment_t>(spec_ibl_init.color_attachment_count);

    VkExtent2D extent2d = {};
    extent2d.width = 512;
    extent2d.height = 512;

    spec_ibl_init.color_attachments[0].init_as_color_attachment(extent3d, VK_FORMAT_R16G16B16A16_SFLOAT);

    VkAttachmentDescription attachment_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);

    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = spec_ibl_init.color_attachment_count;
    subpass_description.pColorAttachments = &attachment_reference;
    subpass_description.pDepthStencilAttachment = NULL;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = spec_ibl_init.color_attachment_count;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &spec_ibl_init.render_pass));

    spec_ibl_init.framebuffer = create_framebuffer(
        spec_ibl_init.color_attachment_count,
        spec_ibl_init.color_attachments,
        NULL,
        spec_ibl_init.render_pass,
        extent2d,
        1);

    const char *paths[] = {
        "shaders/SPV/specular_ibl.vert.spv",
        "shaders/SPV/specular_ibl.frag.spv"
    };

    VkDescriptorType input_types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };

    shader_create_info_t info;
    info.push_constant_size = sizeof(spec_ibl_render_data_t);
    info.descriptor_layout_types = input_types;
    info.descriptor_layout_count = sizeof(input_types) / sizeof(input_types[0]);
    info.shader_paths = paths;
    info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    spec_ibl_init_shader.init_as_2d_shader(&info, &spec_ibl_init);
}

void pbr_environment_t::render_to_spec_ibl(VkDescriptorSet base_cubemap, VkCommandBuffer cmdbuf) {
    VkImageMemoryBarrier barrier = create_image_barrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        spec_ibl_cubemap.image,
        0, 6,
        0, 5,
        VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdPipelineBarrier(
        cmdbuf,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);
    
    matrix4_t projection_matrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

    matrix4_t view_matrices[6] = {
        glm::rotate(glm::rotate(matrix4_t(1.0f), glm::radians(90.0f), vector3_t(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), vector3_t(1.0f, 0.0f, 0.0f)),
        glm::rotate(glm::rotate(matrix4_t(1.0f), glm::radians(-90.0f), vector3_t(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), vector3_t(1.0f, 0.0f, 0.0f)),
        glm::rotate(matrix4_t(1.0f), glm::radians(-90.0f), vector3_t(1.0f, 0.0f, 0.0f)),
        glm::rotate(matrix4_t(1.0f), glm::radians(90.0f), vector3_t(1.0f, 0.0f, 0.0f)),
        glm::rotate(matrix4_t(1.0f), glm::radians(180.0f), vector3_t(1.0f, 0.0f, 0.0f)),
        glm::rotate(matrix4_t(1.0f), glm::radians(180.0f), vector3_t(0.0f, 0.0f, 1.0f)),
    };

    spec_ibl_render_data_t render_data = {};

    for (uint32_t mip = 0; mip < 5; ++mip) {
        uint32_t width = (uint32_t)(512.0f * pow(0.5, mip));
        uint32_t height = (uint32_t)(512.0f * pow(0.5, mip));

        float roughness = (float)mip / (float)(5 - 1);
        
        for (uint32_t layer = 0; layer < 6; ++layer) {
            VkClearValue clear_value = {};
    
            VkRect2D render_area = {};
            render_area.extent.width = 512;
            render_area.extent.height = 512;
    
            VkRenderPassBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            begin_info.renderPass = spec_ibl_init.render_pass;
            begin_info.framebuffer = spec_ibl_init.framebuffer;
            begin_info.renderArea = render_area;
            begin_info.clearValueCount = 1;
            begin_info.pClearValues = &clear_value;

            vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport = {};
            viewport.width = (float)width;
            viewport.height = (float)height;
            viewport.maxDepth = 1;
            vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

            VkRect2D rect = {};
            rect.extent.width = width;
            rect.extent.height = height;
            vkCmdSetScissor(cmdbuf, 0, 1, &rect);

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, spec_ibl_init_shader.pipeline);

            vkCmdBindDescriptorSets(
                cmdbuf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                spec_ibl_init_shader.layout,
                0,
                //1, &base_cubemap_descriptor_set,
                1, &base_cubemap,
                0, NULL);

            render_data.face_rotation = projection_matrix * view_matrices[layer];
            render_data.roughness = roughness;
            render_data.layer = (float)layer;

            vkCmdPushConstants(
                cmdbuf,
                spec_ibl_init_shader.layout,
                spec_ibl_init_shader.flags,
                0,
                sizeof(spec_ibl_render_data_t),
                &render_data);

            vkCmdDraw(cmdbuf, 36, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);

            barrier = create_image_barrier(
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                spec_ibl_init.color_attachments[0].image,
                0, 1,
                0, 1,
                VK_IMAGE_ASPECT_COLOR_BIT);

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier);

            VkImageCopy region = {};
            region.extent.width = width;
            region.extent.height = height;
            region.extent.depth = 1;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.mipLevel = mip;
            region.dstSubresource.baseArrayLayer = layer;
            region.dstSubresource.layerCount = 1;

            vkCmdCopyImage(
                cmdbuf,
                spec_ibl_init.color_attachments[0].image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                spec_ibl_cubemap.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region);

            barrier = create_image_barrier(
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                spec_ibl_init.color_attachments[0].image,
                0, 1,
                0, 1,
                VK_IMAGE_ASPECT_COLOR_BIT);

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier);

            barrier = create_image_barrier(
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                spec_ibl_cubemap.image,
                layer, 1,
                mip, 1,
                VK_IMAGE_ASPECT_COLOR_BIT);

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, &barrier);
        }
    }
}

}
