#include "r_internal.hpp"

struct lighting_data_t {
    vector4_t light_positions[4];
    vector4_t light_colors[4];
};

static lighting_data_t lighting_data;

static gpu_buffer_t light_uniform_buffer;
static VkDescriptorSet light_descriptor_set;

VkDescriptorSet r_lighting_uniform() {
    return light_descriptor_set;
}

void r_lighting_init() {
    memset(&lighting_data, 0, sizeof(lighting_data));

    gpu_camera_transforms_t *transforms = r_gpu_camera_data();
    
    lighting_data.light_positions[0] = transforms->view * vector4_t(-10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[1] = transforms->view * vector4_t(10.0f, 10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[2] = transforms->view * vector4_t(-10.0f, -10.0f, 10.0f, 0.0f);
    lighting_data.light_positions[3] = transforms->view * vector4_t(10.0f, -10.0f, 10.0f, 0.0f);

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
