#pragma once

#include "vk_shader.hpp"
#include "vk_buffer.hpp"

#include <stdint.h>

namespace vk {

enum buffer_type_t : char {
    BT_INVALID_BUFFER_TYPE,
    BT_INDICES,
    BT_VERTEX,
    BT_NORMAL,
    BT_UVS,
    BT_COLOR,
    BT_JOINT_WEIGHT,
    BT_JOINT_INDICES,
    BT_EXTRA_V3,
    BT_EXTRA_V2,
    BT_EXTRA_V1,
    BT_MAX_VALUE
};

struct mesh_buffer_t {
    gpu_buffer_t gpu_buffer;
    buffer_type_t type;
};

constexpr uint32_t MAX_MESH_BUFFERS = BT_MAX_VALUE;

struct mesh_t {
    mesh_buffer_t buffers[MAX_MESH_BUFFERS];
    uint32_t buffer_count;
    buffer_type_t buffer_type_stack[BT_MAX_VALUE];

    // This is what will get passed to the vkCmdBindVertexBuffers
    uint32_t vertex_buffer_count;
    VkBuffer vertex_buffers_final[BT_MAX_VALUE];
    VkDeviceSize vertex_buffers_offsets[BT_MAX_VALUE];
    VkBuffer index_buffer;

    // Data needed to render
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t index_offset;
    VkIndexType index_type;

    void push_buffer(buffer_type_t buffer_type);
    bool has_buffer(buffer_type_t buffer_type);
    mesh_buffer_t *get_mesh_buffer(buffer_type_t buffer_type);
    shader_binding_info_t create_shader_binding_info();
    void init_mesh_vbo_final_list();

    // Load vertices, indices, etc...
    void load_sphere(shader_binding_info_t *shader_info);
    void load_cube(shader_binding_info_t *shader_info);
    void load_external(shader_binding_info_t *shader_info, const char *path);
};

void create_player_merged_mesh(
    mesh_t *dst_a, shader_binding_info_t *sbi_a,
    mesh_t *dst_b, shader_binding_info_t *sbi_b,
    mesh_t *dst_merged, shader_binding_info_t *sbi_merged);

struct mesh_render_data_t {
    matrix4_t model;
    vector4_t color;

    // .x = roughness
    // .y = metallic
    vector4_t pbr_info;
    
    // To add later with texture stuff
    int32_t texture_index;
};

constexpr uint32_t DEF_MESH_RENDER_DATA_SIZE = sizeof(mesh_render_data_t);

// By default, a uniform buffer is added for camera transforms
// If STATIC is chosen, no extra descriptors will be added to shader information
// If ANIMATED is chosen, 1 extra uniform buffer will be added for joint transforms
// If PASS_EXTRA_UNIFORM_BUFFER, all uniforms will be added from previous options, and
// Extra ones will be added that you pass in the extra descriptors array
enum mesh_type_t {
    MT_STATIC = 1 << 0,
    MT_ANIMATED = 1 << 1,
    MT_PASS_EXTRA_UNIFORM_BUFFER = 1 << 2,
    MT_MERGED_MESH = 1 << 3
};

typedef int32_t mesh_type_flags_t;

shader_t create_mesh_shader_color(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling,
    VkPrimitiveTopology topology,
    mesh_type_flags_t type);

shader_t create_mesh_shader_shadow(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    mesh_type_flags_t mesh_type);

// Submitting meshes to GPU
void submit_mesh(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data);
void submit_mesh_shadow(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data);
void submit_skeletal_mesh(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    struct animated_instance_t *instance);

}
