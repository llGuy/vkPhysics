#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <common/tools.hpp>

typedef int32_t result_t;
typedef void(*surface_proc_t)(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window);

#if LINK_AGAINST_RENDERER
#define DECLARE_RENDERER_PROC(return_type, name, ...)   \
    return_type name(__VA_ARGS__)
#define DECLARE_POINTER_RENDERER_PROC(return_type, name, ...) \
    return_type name(__VA_ARGS__)
#define DECLARE_VOID_RENDERER_PROC(return_type, name, ...)   \
    return_type name(__VA_ARGS__)
#else
#define DECLARE_RENDERER_PROC(return_type, name, ...)   \
    inline return_type name(__VA_ARGS__) { return return_type{}; }
#define DECLARE_POINTER_RENDERER_PROC(return_type, name, ...) \
    inline return_type name(__VA_ARGS__) {return NULL;}
#define DECLARE_VOID_RENDERER_PROC(return_type, name, ...)   \
    inline return_type name(__VA_ARGS__) {}
#endif

/* Initialise the renderer (graphics API, rendering pipeline, mesh managers, ...) */
DECLARE_VOID_RENDERER_PROC(void, renderer_init,
    const char *application_name,
    surface_proc_t create_surface,
    void *window,
    uint32_t window_width,
    uint32_t window_height);

DECLARE_VOID_RENDERER_PROC(void, destroy_renderer,
    void);

/* Swapchain */
DECLARE_VOID_RENDERER_PROC(void, handle_resize,
    uint32_t width,
    uint32_t height);

struct swapchain_information_t {
    uint32_t frames_in_flight;
    uint32_t image_count;
    uint32_t width;
    uint32_t height;
};

DECLARE_VOID_RENDERER_PROC(void, swapchain_information,
    swapchain_information_t *dst);

/* Rendering begin / end */
DECLARE_RENDERER_PROC(VkCommandBuffer, begin_frame, void);

struct eye_3d_info_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    float fov;
    float near;
    float far;
    float dt;
};

#define MAX_LIGHTS 10

struct lighting_info_t {
    vector4_t ws_light_positions[MAX_LIGHTS];
    vector4_t ws_light_directions[MAX_LIGHTS];
    vector4_t light_colors[MAX_LIGHTS];
    uint32_t lights_count;

    vector4_t ws_directional_light;
};

DECLARE_VOID_RENDERER_PROC(void, render_environment,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, gpu_data_sync,
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye,
    lighting_info_t *lighting);

DECLARE_VOID_RENDERER_PROC(void, end_frame, void);

DECLARE_VOID_RENDERER_PROC(void, begin_scene_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, end_scene_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, begin_shadow_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, end_shadow_rendering,
    VkCommandBuffer command_buffer);

/* Command buffer utility */
DECLARE_VOID_RENDERER_PROC(void, create_command_buffers,
    VkCommandBufferLevel level, 
    VkCommandBuffer *command_buffers, uint32_t count);

DECLARE_VOID_RENDERER_PROC(void, fill_main_inheritance_info,
    VkCommandBufferInheritanceInfo *info);

DECLARE_VOID_RENDERER_PROC(void, begin_command_buffer,
    VkCommandBuffer command_buffer,
    VkCommandBufferUsageFlags usage,
    VkCommandBufferInheritanceInfo *inheritance);

DECLARE_VOID_RENDERER_PROC(void, end_command_buffer,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, submit_secondary_command_buffer,
    VkCommandBuffer pcommand_buffer,
    VkCommandBuffer scommand_buffer);

DECLARE_RENDERER_PROC(VkCommandBuffer, begin_single_time_command_buffer, void);

DECLARE_VOID_RENDERER_PROC(void, end_single_time_command_buffer,
    VkCommandBuffer command_buffer);

/* Images */
DECLARE_RENDERER_PROC(VkDeviceMemory, allocate_image_memory,
    VkImage image,
    VkMemoryPropertyFlags properties);

/* Buffers */
DECLARE_RENDERER_PROC(VkDeviceMemory, allocate_gpu_buffer_memory,
    VkBuffer buffer,
    VkMemoryPropertyFlags properties);

struct gpu_buffer_t {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkDeviceSize size;
};

DECLARE_RENDERER_PROC(gpu_buffer_t, create_gpu_buffer,
    uint32_t size,
    void *data,
    VkBufferUsageFlags usage);

DECLARE_VOID_RENDERER_PROC(void, update_gpu_buffer,
    VkCommandBuffer command_buffer,
    VkPipelineStageFlags pipeline_stage,
    uint32_t offset,
    uint32_t data_size,
    void *data,
    gpu_buffer_t *gpu_buffer);

DECLARE_VOID_RENDERER_PROC(void, destroy_gpu_buffer,
    gpu_buffer_t gpu_buffer);

#define MAX_SENSITIVE_DELETIONS 1000

// Pushes this buffer to a queue which 
DECLARE_VOID_RENDERER_PROC(void, destroy_sensitive_gpu_buffer,
    gpu_buffer_t gpu_buffer);

/* Pipeline barriers */
DECLARE_RENDERER_PROC(VkImageMemoryBarrier, create_image_barrier,
    VkImageLayout old_image_layout,
    VkImageLayout new_image_layout,
    VkImage image,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_mip,
    uint32_t mip_levels,
    VkImageAspectFlags aspect);

DECLARE_RENDERER_PROC(VkBufferMemoryBarrier, create_gpu_buffer_barrier,
    VkPipelineStageFlags src,
    VkPipelineStageFlags dst,
    gpu_buffer_t *buffer,
    uint32_t offset,
    uint32_t max);

/* Mesh */
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

#define MAX_MESH_BUFFERS BT_MAX_VALUE

// For the data needed to render (vertex offset, first index, index_count, index_offset, index_type) - will be set to default values by load_mesh_* functions
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
};

DECLARE_VOID_RENDERER_PROC(void, push_buffer_to_mesh,
    buffer_type_t buffer_type,
    mesh_t *mesh);

DECLARE_VOID_RENDERER_PROC(bool, mesh_has_buffer,
    buffer_type_t buffer_type,
    mesh_t *mesh);

DECLARE_POINTER_RENDERER_PROC(mesh_buffer_t *, get_mesh_buffer,
    buffer_type_t buffer_type,
    mesh_t *mesh);

DECLARE_VOID_RENDERER_PROC(void, create_mesh_vbo_final_list,
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

DECLARE_VOID_RENDERER_PROC(void, load_mesh_internal,
    internal_mesh_type_t mesh_type,
    mesh_t *mesh,
    shader_binding_info_t *info);

DECLARE_VOID_RENDERER_PROC(void, load_mesh_external,
    mesh_t *mesh,
    shader_binding_info_t *info,
    const char *path);

#define MAX_CHILD_JOINTS 5

struct joint_t {
    uint32_t joint_id = 0;
    // Eventually won't need joint name
    const char *joint_name;
    uint32_t children_count = 0;
    uint32_t children_ids[MAX_CHILD_JOINTS] = {};
    // Inverse of transform going from model space origin to "bind" position of bone
    // "bind" = position of bone by default (without any animations affecting it)
    matrix4_t inverse_bind_transform;
};

struct skeleton_t {
    uint32_t joint_count;
    joint_t *joints;
};

DECLARE_VOID_RENDERER_PROC(void, load_skeleton,
    skeleton_t *skeleton,
    const char *path);

#define MAX_ANIMATION_CYCLES 10

struct joint_position_key_frame_t {
    vector3_t position;
    float time_stamp;
};

struct joint_rotation_key_frame_t {
    quaternion_t rotation;
    float time_stamp;
};

struct joint_scale_key_frame_t {
    vector3_t scale;
    float time_stamp;
};

// Index of the joint_key_frames_t in the array = joint id
struct joint_key_frames_t {
    uint32_t position_count;
    joint_position_key_frame_t *positions;
    uint32_t rotation_count;
    joint_rotation_key_frame_t *rotations;
    uint32_t scale_count;
    joint_scale_key_frame_t *scales;
};

struct animation_cycle_t {
    const char *animation_name;
    
    float duration;
    uint32_t joint_animation_count;
    joint_key_frames_t *joint_animations;
};

struct animation_cycles_t {
    uint32_t cycle_count;
    animation_cycle_t *cycles;
};

DECLARE_VOID_RENDERER_PROC(void, load_animation_cycles,
    animation_cycles_t *cycles,
    const char *path);

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
DECLARE_RENDERER_PROC(shader_t, create_2d_shader,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    struct rpipeline_stage_t *stage,
    VkPrimitiveTopology topology);

// Enables depth and will always happen in deferred stage
DECLARE_RENDERER_PROC(shader_t, create_3d_shader_shadow,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags);

DECLARE_RENDERER_PROC(shader_t, create_3d_shader_color,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling);

DECLARE_RENDERER_PROC(shader_t, create_mesh_shader_color,
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling);

DECLARE_RENDERER_PROC(shader_t, create_mesh_shader_shadow,
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags);

DECLARE_RENDERER_PROC(shader_binding_info_t, create_mesh_binding_info,
    mesh_t *mesh);

DECLARE_VOID_RENDERER_PROC(void, free_mesh_binding_info,
    shader_binding_info_t *);

// Submits only one mesh
DECLARE_VOID_RENDERER_PROC(void, submit_mesh,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t *render_data);

DECLARE_VOID_RENDERER_PROC(void, submit_mesh_shadow,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    mesh_render_data_t * render_data);

/* Descriptor set */
DECLARE_RENDERER_PROC(VkDescriptorSet, create_image_descriptor_set,
    VkImageView image,
    VkSampler sampler,
    VkDescriptorType type);

DECLARE_RENDERER_PROC(VkDescriptorSet, create_image_descriptor_set,
    VkImageView *images,
    VkSampler *samplers,
    uint32_t count,
    VkDescriptorType type);

DECLARE_RENDERER_PROC(VkDescriptorSet, create_buffer_descriptor_set,
    VkBuffer buffer,
    VkDeviceSize buffer_size,
    VkDescriptorType type);

DECLARE_RENDERER_PROC(VkDescriptorSet, create_buffer_descriptor_set,
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

DECLARE_RENDERER_PROC(texture_t, create_texture,
    const char *path,
    VkFormat format,
    void *data,
    uint32_t width, uint32_t height,
    VkFilter filter);

// Debug stuff
typedef void (*debug_ui_proc_t)();
DECLARE_VOID_RENDERER_PROC(void, add_debug_ui_proc,
    debug_ui_proc_t proc);
