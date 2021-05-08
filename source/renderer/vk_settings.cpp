#include <vk.hpp>
#include "vk_sync.hpp"

namespace vk {

graphics_settings_t g_gfx_settings;

static struct {
    gpu_buffer_t ubo;
    VkDescriptorSet descriptor;
} settings_data;

void load_settings() {
    g_gfx_settings.is_shadow_enabled = 1;
    g_gfx_settings.is_god_rays_enabled = 1;
}

void init_settings() {
    load_settings();

    settings_data.ubo.init(
        sizeof(graphics_settings_t),
        &g_gfx_settings,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    settings_data.descriptor = create_buffer_descriptor_set(
        settings_data.ubo.buffer,
        sizeof(graphics_settings_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void sync_settings(VkCommandBuffer transfer) {
    VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        sizeof(graphics_settings_t),
        &settings_data.ubo);

    vkCmdPipelineBarrier(
        transfer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);

    vkCmdUpdateBuffer(
        transfer,
        settings_data.ubo.buffer,
        0,
        sizeof(graphics_settings_t),
        &g_gfx_settings);

    barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        sizeof(graphics_settings_t),
        &settings_data.ubo);

    vkCmdPipelineBarrier(
        transfer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);
}

VkDescriptorSet get_settings_descriptor() {
    return settings_data.descriptor;
}

}
