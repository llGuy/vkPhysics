#include "vk.hpp"
#include "vk_sync.hpp"
#include "vk_imgui.hpp"
#include "vk_shader.hpp"
#include "vk_buffer.hpp"
#include "vk_scene3d.hpp"
#include "vk_context.hpp"
#include "app_context.hpp"
#include "vk_scene3d_camera.hpp"
#include "vk_scene3d_lighting.hpp"
#include "vk_scene3d_environment.hpp"

#include <imgui.h>
#include <vulkan/vulkan_core.h>

namespace vk {

static struct {
    gpu_buffer_t transforms_ubo;
    VkDescriptorSet descriptor_set;
    camera_ubo_data_t transforms;
    camera_info_t info;
} camera_data;

void prepare_scene3d_camera() {
    camera_info_t *c = &camera_data.info;

    c->position = vector3_t(0.0f, 5.0f, 0.0f);
    c->direction = vector3_t(1.0f, 0.0, 0.0f);
    c->up = vector3_t(0.0f, 1.0f, 0.0f);
    c->fov = 60.0f;
    c->near = 0.1f;
    c->far = 10000.0f;
    
    double x, y;
    app::get_immediate_cursor_pos(&x, &y);
    c->mouse_position = vector2_t((float)x, (float)y);

    camera_ubo_data_t *u = &camera_data.transforms;

    VkExtent2D extent = g_ctx->swapchain.extent;

    u->projection = glm::perspective(
        glm::radians(c->fov),
        (float)extent.width / (float)extent.height,
        c->near,
        c->far);

    // Due to Vulkan
    u->projection[1][1] *= -1.0f;

    u->view = glm::lookAt(c->position, c->position + c->direction, c->up);
    u->inverse_view = glm::inverse(u->view);
    u->view_projection = u->projection * u->view;

    c->previous_view_projection = u->previous_view_projection = u->view_projection * u->inverse_view;
    u->frustum.x = c->near;
    u->frustum.y = c->far;
    u->view_direction = vector4_t(c->direction, 1.0f);
    u->dt = 1.0f / 60.0f;
    u->width = (float)extent.width;
    u->height = (float)extent.height;
    
    camera_data.transforms_ubo.init(
        sizeof(camera_ubo_data_t),
        &camera_data.transforms,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    camera_data.descriptor_set = create_buffer_descriptor_set(
        camera_data.transforms_ubo.buffer,
        camera_data.transforms_ubo.size,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

static void s_camera_gpu_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye_info) {
    camera_data.transforms.dt = eye_info->dt;

    camera_data.info.position = eye_info->position;
    camera_data.info.direction = eye_info->direction;
    camera_data.info.up = eye_info->up;
    camera_data.info.fov = eye_info->fov;
    camera_data.info.near = eye_info->near;
    camera_data.info.far = eye_info->far;
    
    camera_data.transforms.projection = glm::perspective(
        glm::radians(camera_data.info.fov),
        (float)g_ctx->swapchain.extent.width / (float)g_ctx->swapchain.extent.height,
        camera_data.info.near,
        camera_data.info.far);

    camera_data.transforms.projection[1][1] *= -1.0f;

    camera_data.transforms.view = glm::lookAt(
        camera_data.info.position,
        camera_data.info.position + camera_data.info.direction,
        camera_data.info.up);

    camera_data.transforms.inverse_view = glm::inverse(camera_data.transforms.view);

    camera_data.transforms.view_projection = camera_data.transforms.projection * camera_data.transforms.view;
    VkExtent2D extent = g_ctx->swapchain.extent;
    camera_data.transforms.width = (float)extent.width;
    camera_data.transforms.height = (float)extent.height;

    camera_data.transforms.previous_view_projection =
        camera_data.transforms.previous_view_projection *
        camera_data.transforms.inverse_view;

    VkBufferMemoryBarrier buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        (uint32_t)camera_data.transforms_ubo.size,
        &camera_data.transforms_ubo);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);
    
    vkCmdUpdateBuffer(
        command_buffer,
        camera_data.transforms_ubo.buffer,
        0,
        camera_data.transforms_ubo.size,
        &camera_data.transforms);

    buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        (uint32_t)camera_data.transforms_ubo.size,
        &camera_data.transforms_ubo);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);

    camera_data.transforms.previous_view_projection = camera_data.transforms.view_projection;
}

static struct {
    lighting_ubo_data_t ubo_data;
    vector3_t directional_light;
    gpu_buffer_t ubo;
    VkDescriptorSet descriptor;
    shadow_box_t shadow;
} lighting_data;

void prepare_scene3d_lighting() {
    memset(&lighting_data, 0, sizeof(lighting_data));

    camera_ubo_data_t *c_transforms = &camera_data.transforms;

    lighting_data.directional_light = vector3_t(0.1f, 0.422f, 0.714f);

    lighting_data.ubo_data.ws_light_positions[0] = vector4_t(0.0f, 4.0f, 0.0f, 1.0f);
    lighting_data.ubo_data.light_positions[0] = c_transforms->view * lighting_data.ubo_data.ws_light_positions[0];

    lighting_data.ubo_data.ws_light_directions[0] = glm::normalize(vector4_t(0.0f, -0.3f, 0.8f, 0.0f));
    lighting_data.ubo_data.light_directions[0] = c_transforms->view * lighting_data.ubo_data.ws_light_directions[0];
    
    lighting_data.ubo_data.light_colors[0] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);
    
    lighting_data.ubo_data.ws_directional_light = -vector4_t(lighting_data.directional_light, 0.0f);
    lighting_data.ubo_data.vs_directional_light = c_transforms->view * lighting_data.ubo_data.ws_directional_light;

    lighting_data.ubo_data.point_light_count = 0;

    lighting_data.shadow.init(lighting_data.directional_light, vector3_t(0.0f, 1.0f, 0.0f), 25.0f);
    
    lighting_data.ubo.init(
        sizeof(lighting_ubo_data_t),
        &lighting_data,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    lighting_data.descriptor = create_buffer_descriptor_set(
        lighting_data.ubo.buffer,
        sizeof(lighting_ubo_data_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

static void s_lighting_gpu_sync(lighting_info_t *info, eye_3d_info_t *eye, VkCommandBuffer cmdbuf) {
    { // Do math
        auto *c_transforms = &camera_data.transforms;
        auto *c_info = &camera_data.info;

        for (uint32_t i = 0; i < info->lights_count; ++i) {
            lighting_data.ubo_data.ws_light_positions[i] = info->ws_light_positions[i];
            lighting_data.ubo_data.light_positions[i] = c_transforms->view * info->ws_light_positions[i];
            lighting_data.ubo_data.ws_light_directions[i] = info->ws_light_directions[i];
            lighting_data.ubo_data.light_directions[i] = c_transforms->view * info->ws_light_directions[i];
            lighting_data.ubo_data.light_colors[i] = info->light_colors[i];
        }

        lighting_data.ubo_data.point_light_count = info->lights_count;

        lighting_data.shadow.update(
            lighting_data.directional_light,
            glm::radians(fmax(eye->fov, 60.0f)),
            c_transforms->width / c_transforms->height,
            c_info->position,
            c_info->direction,
            c_info->up);

        matrix4_t view_rotation = c_transforms->view;
        view_rotation[3][0] = 0.0f;
        view_rotation[3][1] = 0.0f;
        view_rotation[3][2] = 0.0f;
    
        lighting_data.ubo_data.ws_directional_light = -glm::normalize(info->ws_directional_light);
        lighting_data.ubo_data.vs_directional_light = view_rotation * lighting_data.ubo_data.ws_directional_light;

        vector4_t light_position = vector4_t(lighting_data.directional_light * 1000000.0f, 0.0f);
    
        light_position = c_transforms->projection * view_rotation * light_position;
        light_position.x /= light_position.w;
        light_position.y /= light_position.w;
        light_position.z /= light_position.w;
        light_position.x = light_position.x * 0.5f + 0.5f;
        light_position.y = light_position.y * 0.5f + 0.5f;
        lighting_data.ubo_data.light_screen_coord = vector2_t(light_position.x, light_position.y);

        lighting_data.ubo_data.shadow_view = lighting_data.shadow.view;
        lighting_data.ubo_data.shadow_projection = lighting_data.shadow.projection;
        lighting_data.ubo_data.shadow_view_projection = lighting_data.shadow.projection * lighting_data.shadow.view;
    }

    { // Actually sync with GPU
        VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            sizeof(lighting_ubo_data_t),
            &lighting_data.ubo);

        vkCmdPipelineBarrier(
            cmdbuf,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            1, &barrier,
            0, NULL);

        vkCmdUpdateBuffer(
            cmdbuf,
            lighting_data.ubo.buffer,
            0,
            sizeof(lighting_ubo_data_t),
            &lighting_data);

        barrier = create_gpu_buffer_barrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            sizeof(lighting_ubo_data_t),
            &lighting_data.ubo);

        vkCmdPipelineBarrier(
            cmdbuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            0, NULL,
            1, &barrier,
            0, NULL);
    }
}

static struct {
    atmosphere_model_t atm;

    VkExtent2D extent;
    // Base because there will be IBL cubemaps, etc...
    render_pipeline_stage_t base_cubemap_init;
    // To create the cubemap
    shader_t base_cubemap_init_shader;

    // To render the cubemap in the world
    shader_t cubemap_shader;

    // IBL components, etc...
    pbr_environment_t pbr_env;
} environment;

static void s_init_environment_cubemap() {
    environment.extent.width = 512;
    environment.extent.height = 512;

    VkExtent3D extent3d = {};
    extent3d.width = environment.extent.width;
    extent3d.height = environment.extent.height;
    extent3d.depth = 1;

    environment.base_cubemap_init.color_attachment_count = 1;
    environment.base_cubemap_init.color_attachments = FL_MALLOC(attachment_t, environment.base_cubemap_init.color_attachment_count);

    environment.base_cubemap_init.color_attachments[0].init_as_cubemap(
        extent3d,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        1,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VkAttachmentDescription cubemap_description = fill_color_attachment_description(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT);
    
    VkAttachmentReference cubemap_reference = {};
    cubemap_reference.attachment = 0;
    cubemap_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = environment.base_cubemap_init.color_attachment_count;
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
    render_pass_info.attachmentCount = environment.base_cubemap_init.color_attachment_count;
    render_pass_info.pAttachments = &cubemap_description;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &render_pass_info, NULL, &environment.base_cubemap_init.render_pass));

    environment.base_cubemap_init.framebuffer = create_framebuffer(
        environment.base_cubemap_init.color_attachment_count,
        environment.base_cubemap_init.color_attachments,
        NULL,
        environment.base_cubemap_init.render_pass,
        environment.extent,
        6);

    environment.base_cubemap_init.init_descriptor_set_output();

    const char *paths[] = {
        "shaders/SPV/atmosphere_scatter_cubemap_init.vert.spv",
        "shaders/SPV/atmosphere_scatter_cubemap_init.geom.spv",
        "shaders/SPV/atmosphere_scatter_cubemap_init.frag.spv"
    };

    shader_create_info_t info;
    info.push_constant_size = sizeof(atmosphere_model_t);
    info.shader_paths = paths;
    info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    environment.base_cubemap_init_shader.init_as_2d_shader(&info, &environment.base_cubemap_init);
}

static void s_render_to_base_cubemap(VkCommandBuffer cmdbuf) {
    VkClearValue clear_value = {};
    
    VkRect2D render_area = {};
    render_area.extent = environment.extent;
    
    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = environment.base_cubemap_init.render_pass;
    begin_info.framebuffer = environment.base_cubemap_init.framebuffer;
    begin_info.renderArea = render_area;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;
    
    vkCmdBeginRenderPass(cmdbuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = (float)environment.extent.width;
    viewport.height = (float)environment.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent.width = environment.extent.width;
    rect.extent.height = environment.extent.height;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, environment.base_cubemap_init_shader.pipeline);

    matrix4_t projection = glm::perspective(
        glm::radians(90.0f),
        (float)environment.extent.width / (float)environment.extent.height,
        0.1f, 5.0f);
    
    environment.atm.width = (float)environment.extent.width;
    environment.atm.height = (float)environment.extent.height;
    environment.atm.inverse_projection = glm::inverse(projection);
    
    vkCmdPushConstants(
        cmdbuf,
        environment.base_cubemap_init_shader.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(atmosphere_model_t),
        &environment.atm);

    vkCmdDraw(cmdbuf, 1, 1, 0, 0);

    vkCmdEndRenderPass(cmdbuf);
}

struct cubemap_render_data_t {
    matrix4_t model;
    float invert_y;
};

static void s_init_cubemap_shader() {
    shader_binding_info_t binding_info = {};
    VkDescriptorType descriptor_types[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    const char *shader_paths[] = {
        "shaders/SPV/cubemap.vert.spv",
        "shaders/SPV/cubemap.frag.spv"
    };

    shader_create_info_t info;
    info.binding_info = &binding_info;
    info.push_constant_size = sizeof(cubemap_render_data_t);
    info.descriptor_layout_types = descriptor_types;
    info.descriptor_layout_count = sizeof(descriptor_types) / sizeof(descriptor_types[0]);
    info.shader_paths = shader_paths;
    info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.cull_mode = VK_CULL_MODE_NONE;
    
    environment.cubemap_shader.init_as_3d_shader_for_stage(&info, ST_DEFERRED);
}

static integral_lut_t integral_lut;

static bool updated_environment = 0;

static void s_scene_debug_menu() {
    ImGui::Separator();
    ImGui::Text("-- Environment --");

    static float eye_height = 0.0f;
    ImGui::SliderFloat("Eye height", &eye_height, 0.0f, 1.0f);

    static float light_direction[3] = { 0.1f, 0.422f, 0.714f };
    ImGui::SliderFloat3("Light direction", light_direction, -1.0f, +1.0f);

    static float rayleigh = -0.082f;
    ImGui::SliderFloat("Rayleigh factor", &rayleigh, -0.1f, 0.0f);
    
    static float mie = -0.908f;
    ImGui::SliderFloat("Mie factor", &mie, -0.999f, -0.75f);

    static float intensity = 0.650f;
    ImGui::SliderFloat("Intensity", &intensity, 0.1f, 30.0f);

    static float scatter_strength = 1.975f;
    ImGui::SliderFloat("Scatter strength", &scatter_strength, 1.0f, 30.0f);

    static float rayleigh_strength = 2.496f;
    ImGui::SliderFloat("Rayleigh strength", &rayleigh_strength, 0.0f, 3.0f);

    static float mie_strength = 0.034f;
    ImGui::SliderFloat("Mie strength", &mie_strength, 0.0f, 3.0f);

    static float rayleigh_collection = 8.0f;
    ImGui::SliderFloat("Rayleigh collection", &rayleigh_collection, 0.0f, 3.0f);

    static float mie_collection = 2.981f;
    ImGui::SliderFloat("Mie collection", &mie_collection, 0.0f, 3.0f);

    //static float kr[3] = {0.18867780436772762, 0.4978442963618773, 0.6616065586417131};
    static float kr[3] = {27.0f / 255.0f, 82.0f / 255.0f, 111.0f / 255.0f};
    ImGui::ColorEdit3("Air color", kr);

    updated_environment = ImGui::Button("Update");
    
    environment.atm.eye_height = eye_height;
    environment.atm.light_direction_x = light_direction[0];
    environment.atm.light_direction_y = light_direction[1];
    environment.atm.light_direction_z = light_direction[2];
    environment.atm.rayleigh = rayleigh;
    environment.atm.mie = mie;
    environment.atm.intensity = intensity;
    environment.atm.scatter_strength = scatter_strength;
    environment.atm.rayleigh_strength = rayleigh_strength;
    environment.atm.mie_strength = mie_strength;
    environment.atm.rayleigh_collection = rayleigh_collection;
    environment.atm.mie_collection = mie_collection;
    environment.atm.air_color_r = kr[0];
    environment.atm.air_color_g = kr[1];
    environment.atm.air_color_b = kr[2];

    lighting_data.directional_light = vector3_t(
        environment.atm.light_direction_x = light_direction[0],
        environment.atm.light_direction_y = light_direction[1],
        environment.atm.light_direction_z = light_direction[2]);
}

void prepare_scene3d_environment() {
    add_debug_ui_proc(s_scene_debug_menu);

    atmosphere_model_default_values(&environment.atm, lighting_data.directional_light);
    s_init_environment_cubemap();

    environment.pbr_env.prepare_diff_ibl();
    s_init_cubemap_shader();
    integral_lut.init();
    environment.pbr_env.prepare_spec_ibl();

    VkCommandBuffer cmdbuf = begin_single_use_command_buffer();
    s_render_to_base_cubemap(cmdbuf);
    environment.pbr_env.render_to_diff_ibl(environment.base_cubemap_init.descriptor_set, cmdbuf);
    environment.pbr_env.render_to_spec_ibl(environment.base_cubemap_init.descriptor_set, cmdbuf);
    end_single_use_command_buffer(cmdbuf);
}

static void s_update_environment_if_updated(VkCommandBuffer cmdbuf) {
    if (updated_environment) {
        s_render_to_base_cubemap(cmdbuf);
        environment.pbr_env.render_to_diff_ibl(environment.base_cubemap_init.descriptor_set, cmdbuf);
        environment.pbr_env.render_to_spec_ibl(environment.base_cubemap_init.descriptor_set, cmdbuf);

        updated_environment = 0;
    }
}

void gpu_sync_scene3d_data(VkCommandBuffer cmdbuf, eye_3d_info_t *eye, lighting_info_t *light_info) {
    s_camera_gpu_sync(cmdbuf, eye);
    s_lighting_gpu_sync(light_info, eye, cmdbuf);
    s_update_environment_if_updated(cmdbuf);
}

void render_environment(VkCommandBuffer cmdbuf) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, environment.cubemap_shader.pipeline);

    VkDescriptorSet descriptor_sets[] = {
        camera_data.descriptor_set,
        environment.base_cubemap_init.descriptor_set };

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        environment.cubemap_shader.layout,
        0,
        2, descriptor_sets,
        0, NULL);

    cubemap_render_data_t render_data = {};
    render_data.model = glm::translate(camera_data.info.position);
    render_data.invert_y = -1.0f;
    
    vkCmdPushConstants(
        cmdbuf,
        environment.cubemap_shader.layout,
        environment.cubemap_shader.flags,
        0,
        sizeof(cubemap_render_data_t),
        &render_data);

    vkCmdDraw(cmdbuf, 36, 1, 0, 0);
}

scene3d_descriptors_t get_scene_descriptors() {
    scene3d_descriptors_t res = {};
    res.camera_ubo = camera_data.descriptor_set;
    res.diff_ibl_tx = environment.pbr_env.diff_ibl_init.descriptor_set;
    res.spec_ibl_tx = environment.pbr_env.spec_ibl_set;
    res.lighting_ubo = lighting_data.descriptor;
    res.lut_tx = integral_lut.set;

    return res;
}

}
