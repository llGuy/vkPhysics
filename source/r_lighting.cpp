#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "r_internal.hpp"

static lighting_data_t lighting_data;

static vector3_t light_direction;

vector3_t *r_light_direction() {
    return &light_direction;
}

static gpu_buffer_t light_uniform_buffer;
static VkDescriptorSet light_descriptor_set;

VkDescriptorSet r_lighting_uniform() {
    return light_descriptor_set;
}

void r_lighting_init() {
    memset(&lighting_data, 0, sizeof(lighting_data));

    gpu_camera_transforms_t *transforms = r_gpu_camera_data();

    light_direction = vector3_t(0.0f, 0.622f, 0.714f);
    
    lighting_data.light_positions[0] = transforms->view * vector4_t(-10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[1] = transforms->view * vector4_t(10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[2] = transforms->view * vector4_t(-10.0f, -10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[3] = transforms->view * vector4_t(10.0f, -10.0f, 10.0f, 0.0f);
    lighting_data.vs_directional_light = transforms->view * -vector4_t(light_direction, 0.0f);

    lighting_data.light_colors[0] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);
    lighting_data.light_colors[1] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);
    lighting_data.light_colors[2] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);
    lighting_data.light_colors[3] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);

    light_uniform_buffer = create_gpu_buffer(
        sizeof(lighting_data_t),
        &lighting_data,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    light_descriptor_set = create_buffer_descriptor_set(
        light_uniform_buffer.buffer,
        sizeof(lighting_data_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void r_update_lighting() {
    gpu_camera_transforms_t *transforms = r_gpu_camera_data();
    
    lighting_data.light_positions[0] = transforms->view * vector4_t(-10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[1] = transforms->view * vector4_t(10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[2] = transforms->view * vector4_t(-10.0f, -10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[3] = transforms->view * vector4_t(10.0f, -10.0f, 10.0f, 0.0f);
    lighting_data.vs_directional_light = transforms->view * -vector4_t(light_direction, 0.0f);
}

void r_lighting_gpu_sync(VkCommandBuffer command_buffer) {
    VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        &light_uniform_buffer,
        0,
        sizeof(lighting_data_t));

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);

    vkCmdUpdateBuffer(
        command_buffer,
        light_uniform_buffer.buffer,
        0,
        sizeof(lighting_data_t),
        &lighting_data);

    barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        &light_uniform_buffer,
        0,
        sizeof(lighting_data_t));

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);
}
