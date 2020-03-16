#pragma once

#include <stdint.h>
#include "tools.hpp"
#include <vulkan/vulkan.h>

typedef int32_t result_t;
typedef void(*surface_proc_t)(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window);
typedef void(*imgui_proc_t)();

/* Initialise the renderer (graphics API, rendering pipeline, mesh managers, ...) */
void renderer_init(
    const char *application_name,
    surface_proc_t create_surface,
    imgui_proc_t imgui_proc,
    void *window,
    uint32_t window_width,
    uint32_t window_height);

/* Swapchain */
void handle_resize(
    uint32_t width,
    uint32_t height);

/* Rendering begin / end */
VkCommandBuffer begin_frame();

void end_frame();

void begin_scene_rendering(
    VkCommandBuffer command_buffer);

void end_scene_rendering(
    VkCommandBuffer command_buffer);

/* Command buffer utility */
void create_command_buffers(
    VkCommandBufferLevel level, 
    VkCommandBuffer *command_buffers, uint32_t count);

void begin_command_buffer(
    VkCommandBuffer command_buffer,
    VkCommandBufferUsageFlags usage,
    VkCommandBufferInheritanceInfo *inheritance);

void end_command_buffer(
    VkCommandBuffer command_buffer);

VkCommandBuffer begin_single_time_command_buffer();

void end_single_time_command_buffer(
    VkCommandBuffer command_buffer);

/* Images */
VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties);

/* Buffers */
VkDeviceMemory allocate_gpu_buffer_memory(
    VkBuffer buffer,
    VkMemoryPropertyFlags properties);

struct gpu_buffer_t {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkDeviceSize size;
};

gpu_buffer_t create_gpu_buffer(
    uint32_t size,
    void *data,
    VkBufferUsageFlags usage);

/* Pipeline barriers */
VkImageMemoryBarrier create_image_barrier(
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkImage image,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_mip,
    uint32_t mip_levels,
    VkImageAspectFlags aspect);

VkBufferMemoryBarrier create_gpu_buffer_barrier(
    VkPipelineStageFlags src,
    VkPipelineStageFlags dst,
    gpu_buffer_t *buffer,
    uint32_t offset,
    uint32_t max);

/* Mesh */
enum buffer_type_t : char {
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
    BT_INVALID_BUFFER_TYPE
};

struct mesh_buffer_t {
    gpu_buffer_t gpu_buffer;
    buffer_type_t type;
};

#define MAX_MESH_BUFFERS 6

// For the data needed to render (vertex offset, first index, index_count, index_offset, index_type) - will be set to default values by load_mesh_* functions
struct mesh_t {
    mesh_buffer_t buffers[MAX_MESH_BUFFERS];
    uint32_t buffer_count;
    buffer_type_t buffer_type_stack[BT_INVALID_BUFFER_TYPE];

    // This is what will get passed to the vkCmdBindVertexBuffers
    uint32_t vertex_buffer_count;
    VkBuffer vertex_buffers_final[BT_INVALID_BUFFER_TYPE];
    VkDeviceSize vertex_buffers_offsets[BT_INVALID_BUFFER_TYPE];
    VkBuffer index_buffer;

    // Data needed to render
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t index_offset;
    VkIndexType index_type;
};

void push_buffer_to_mesh(
    buffer_type_t buffer_type,
    mesh_t *mesh);

bool mesh_has_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh);

mesh_buffer_t *get_mesh_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh);

enum internal_mesh_type_t {
    IM_SPHERE,
    IM_CUBE
};

struct shader_binding_info_t {
    uint32_t binding_count;
    VkVertexInputBindingDescription *binding_descriptions;

    uint32_t attribute_count;
    VkVertexInputAttributeDescription *attribute_descriptions;
};

void load_mesh_internal(
    internal_mesh_type_t mesh_type,
    mesh_t *mesh,
    shader_binding_info_t *info);

void load_mesh_external();

struct shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkShaderStageFlags flags;
};

struct mesh_render_data_t {
    matrix4_t model;
    vector4_t color;

    // .x = roughness
    // .y = metallic
    vector4_t pbr_info;
    
    // To add later with texture stuff
    int32_t texture_index;
};

// Disabled depth
shader_t create_2d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    struct rpipeline_stage_t *stage,
    VkPrimitiveTopology topology);

// Enables depth and will always happen in deferred stage
shader_t create_3d_shader(
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags);

shader_t create_mesh_shader(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags);

shader_binding_info_t create_mesh_binding_info(
    mesh_t *mesh);

// Submits only one mesh
void submit_mesh(
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data);

/* Descriptor set */
VkDescriptorSet create_image_descriptor_set(
    VkImageView image,
    VkSampler sampler,
    VkDescriptorType type);

VkDescriptorSet create_image_descriptor_set(
    VkImageView *images,
    VkSampler *samplers,
    uint32_t count,
    VkDescriptorType type);

VkDescriptorSet create_buffer_descriptor_set(
    VkBuffer buffer,
    VkDeviceSize buffer_size,
    VkDescriptorType type);

VkDescriptorSet create_buffer_descriptor_set(
    VkBuffer *buffers,
    uint32_t count,
    VkDeviceSize buffer_size,
    VkDescriptorType type);

struct texture_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkSampler sampler;
    VkFormat format;

    VkDescriptorSet descriptor;
};

texture_t create_texture(
    const char *path,
    VkFormat format,
    void *data,
    uint32_t width, uint32_t height,
    VkFilter filter);
