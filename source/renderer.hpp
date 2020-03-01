#pragma once

#include <stdint.h>

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

void resize_swapchain();

VkCommandBuffer begin_frame();

void end_frame();

void begin_scene_rendering(
    VkCommandBuffer command_buffer);

void end_scene_rendering(
    VkCommandBuffer command_buffer);

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

struct attachment_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkFormat format;
    VkSampler sampler;
};
    
VkDeviceMemory allocate_image_memory(
    VkImage image,
    VkMemoryPropertyFlags properties);

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

struct mesh_t {
    mesh_buffer_t buffers[MAX_MESH_BUFFERS];
    uint32_t buffer_count;
    buffer_type_t buffer_type_stack[BT_INVALID_BUFFER_TYPE];

    // This is what will get passed to the vkCmdBindVertexBuffers
    VkBuffer buffers_final;
    VkBuffer index_buffer;

    // Data needed to render
    uint32_t vertex_offset;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t index_offset;
    VkIndexType index_type;
};

void push_buffer_to_mesh(
    buffer_type_t buffer_type,
    mesh_t *mesh);

bool mesh_has_buffer(
    buffer_type_t buffer_type);

mesh_buffer_t *get_mesh_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh);

enum internal_mesh_type_t {
    IM_SPHERE,
    IM_CUBE
};

void load_mesh_internal(
    internal_mesh_type_t mesh_type,
    mesh_t *mesh);

void load_mesh_external();
