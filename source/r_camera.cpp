#include "tools.hpp"
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>

struct gpu_camera_transforms_t {
    matrix4_t projection;
    matrix4_t view;
    matrix4_t view_projection;
};

static gpu_buffer_t transforms_uniform_buffer;
static VkDescriptorSet descriptor_set;
static gpu_camera_transforms_t transforms;

struct cpu_camera_data_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    vector2_t mouse_position;
    float fov;
};

static cpu_camera_data_t camera_data;

void r_camera_init() {
    camera_data.position = vector3_t(0.0f);
    camera_data.direction = vector3_t(1.0f, 0.0, 0.0f);
    camera_data.up = vector3_t(0.0f, 1.0f, 0.0f);
    camera_data.mouse_position = vector2_t(0.0f);
    camera_data.fov = glm::radians(60.0f);

    VkExtent2D extent = r_swapchain_extent();
    transforms.projection = glm::perspective(camera_data.fov, (float)extent.width / (float)extent.height, 0.1f, 100.0f);
    transforms.view = glm::lookAt(camera_data.position, camera_data.position + camera_data.direction, camera_data.up);
    transforms.view_projection = transforms.projection * transforms.view;
    
    transforms_uniform_buffer = create_gpu_buffer(
        sizeof(gpu_camera_transforms_t),
        &transforms,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    descriptor_set = create_buffer_descriptor_set(
        transforms_uniform_buffer.buffer,
        transforms_uniform_buffer.size,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

// Should not worry about case where not running in window mode (server mode) because anyway, r_ prefixed files won't be running
void r_camera_gpu_sync(VkCommandBuffer command_buffer) {
    VkBufferMemoryBarrier buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        &transforms_uniform_buffer,
        0,
        transforms_uniform_buffer.size);

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
        transforms_uniform_buffer.buffer,
        0,
        transforms_uniform_buffer.size,
        &transforms);

    buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        &transforms_uniform_buffer,
        0,
        transforms_uniform_buffer.size);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);
}

// TODO: Add proper input handling
void r_camera_handle_input() {
    
}
