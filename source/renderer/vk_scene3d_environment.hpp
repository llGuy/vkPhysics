#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

#include <math.hpp>

namespace vk {

// Create integral lookup table for PBR rendering
struct integral_lut_t {
    VkDescriptorSet set;

    // In order to clean up the resources after
    attachment_t img;

    void init();
private:
    shader_t init_shader_;
    render_pipeline_stage_t init_;

    void init_render_pass();
    void render_to_lut();
    void cleanup_resources();
};

struct atmosphere_model_t {
    matrix4_t inverse_projection;
    float width;
    float height;
    float light_direction_x;
    float light_direction_y;
    float light_direction_z;
    float rayleigh;
    float mie;
    float intensity;
    float scatter_strength;
    float rayleigh_strength;
    float mie_strength;
    float rayleigh_collection;
    float mie_collection;
    float air_color_r;
    float air_color_g;
    float air_color_b;
};

void atmosphere_model_default_values(atmosphere_model_t *model, const vector3_t &directional_light);

struct pbr_environment_t {
    VkExtent2D extent;

    // DIFFUSE IBL ////////////////////////////////////////////////////////////
    render_pipeline_stage_t diff_ibl_init;
    shader_t diff_ibl_init_shader;

    // Creates framebuffer, render pass, etc...
    void prepare_diff_ibl();

    // base_cubemap will contain the original cubemap texture that will be convoluted
    void render_to_diff_ibl(
        VkDescriptorSet base_cubemap,
        VkCommandBuffer cmdbuf);

    // SPECULAR IBL ///////////////////////////////////////////////////////////
    attachment_t spec_ibl_cubemap;
    VkDescriptorSet spec_ibl_set;
    render_pipeline_stage_t spec_ibl_init;
    shader_t spec_ibl_init_shader;

    void prepare_spec_ibl();
    void render_to_spec_ibl(
        VkDescriptorSet base_cubemap,
        VkCommandBuffer cmdbuf);
};

}
