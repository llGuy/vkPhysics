#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <math.hpp>
#include <vulkan/vulkan_core.h>

#undef near
#undef NEAR
#undef far
#undef FAR

namespace vk {

/*
  Initialises core vulkan objects like the instance, device, but also
  things like the render pipeline, 3D scene objects, etc...
 */
void init_context();

/*
  Resizes all the resources which depend on the size of the window
  (swapchain, all framebuffer targets from render pipeline).
 */
void resize_render_pipeline(uint32_t width, uint32_t height);

/*
  Some helper functions when dealing with command buffers
  pcmdbuf = primary command buffer
  scmdbuf = secondary command buffer
 */
void create_command_buffers(VkCommandBufferLevel level, VkCommandBuffer *cmdbufs, uint32_t count);
void begin_command_buffer(VkCommandBuffer cmdbuf, VkCommandBufferUsageFlags use, VkCommandBufferInheritanceInfo *inher);
void end_command_buffer(VkCommandBuffer command_buffer);
void submit_secondary_command_buffer(VkCommandBuffer pcmdbuf, VkCommandBuffer scmduf);

struct swapchain_information_t {
    uint32_t frames_in_flight;
    uint32_t image_count;
    uint32_t width;
    uint32_t height;
};

swapchain_information_t get_swapchain_info();

/*
  Contains data required to render a frame
  For the moment, just contains a primary command buffer
 */
struct frame_t {
    VkCommandBuffer cmdbuf;
};

/*
  Needs to be called at the beginning of every frame, in order to start rendering
 */
frame_t begin_frame();

/*
  Simply gives some information on the frame like which effects to apply, etc...
  In the future, this will need to contain stuff from the settings e.g. render quality.
 */
struct frame_info_t {
    union {
        struct {
            uint32_t blurred: 1;
            uint32_t ssao: 1;
            uint32_t debug_window: 1;
        };

        uint32_t flags;
    };
};

void end_frame(const frame_info_t *info);

/*
  Describes information that the vk_scene3d files will need in order
  to update certain UBOs that will get sent to the shaders
 */
struct eye_3d_info_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    float fov;
    float near;
    float far;
    float dt;
};

constexpr uint32_t MAX_LIGHTS = 10;

/*
  Describes information that the vk_scene3d files will need in order
  to update certain UBOs that will get sent to the shaders
 */
struct lighting_info_t {
    vector4_t ws_light_positions[MAX_LIGHTS];
    vector4_t ws_light_directions[MAX_LIGHTS];
    vector4_t light_colors[MAX_LIGHTS];
    uint32_t lights_count;

    vector4_t ws_directional_light;
};

/*
  Enumerates the different render pipeline stages
 */
enum stage_type_t {
    ST_SHADOW, ST_DEFERRED, ST_LIGHTING, ST_POST_PROCESS, ST_UI, ST_FINAL, ST_INVALID
};

/*
  When creating a secondary command buffer, need to create this inheritance structure.
  User will need to specify which render pipeline stage this command buffer will be
  Submitted in as a secondary command buffer.
 */
void fill_main_inheritance_info(VkCommandBufferInheritanceInfo *info, stage_type_t type);

enum alpha_blending_t {
    AB_ONE_MINUS_SRC_ALPHA,
    AB_NONE
};

/*
  Provides an easy way to keep track of the vertex attributes of a mesh.
  Is needed to create shaders.
 */
struct shader_binding_info_t {
    uint32_t binding_count;
    VkVertexInputBindingDescription *binding_descriptions;

    uint32_t attribute_count;
    VkVertexInputAttributeDescription *attribute_descriptions;

    void free();
};

struct shader_create_info_t {
    shader_binding_info_t *binding_info;
    uint32_t push_constant_size;
    VkDescriptorType *descriptor_layout_types;
    uint32_t descriptor_layout_count;
    const char **shader_paths;
    VkShaderStageFlags shader_flags;
    VkPrimitiveTopology topology;
    alpha_blending_t alpha_blending;
    VkCullModeFlags cull_mode;

    inline shader_create_info_t() :
        binding_info(NULL),
        push_constant_size(0),
        descriptor_layout_types(NULL),
        descriptor_layout_count(0),
        shader_paths(NULL),
        shader_flags(0),
        topology((VkPrimitiveTopology)0),
        alpha_blending(AB_NONE),
        cull_mode(0) {}
};

/*
  Wrapper structure for Vulkan VkPipeline objects
  Provides easy way to create shaders for different
  stages in the render pipeline.
 */
struct shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkShaderStageFlags flags;

    /* 
       Need to set:
       - shader_create_info_t::binding_info
       - shader_create_info_t::push_constant_size
       - shader_create_info_t::descriptor_layout_types
       - shader_create_info_t::descriptor_layout_count
       - shader_create_info_t::shader_paths
       - shader_create_info_t::shader_flags
       - shader_create_info_t::topology
       - shader_create_info_t::alpha_blending
     */
    void init_as_2d_shader(shader_create_info_t *info, struct render_pipeline_stage_t *stage);
    void init_as_ui_shader(shader_create_info_t *info);

    /* 
       Need to set:
       - shader_create_info_t::binding_info
       - shader_create_info_t::push_constant_size
       - shader_create_info_t::descriptor_layout_types
       - shader_create_info_t::descriptor_layout_count
       - shader_create_info_t::shader_paths
       - shader_create_info_t::shader_flags
       - shader_create_info_t::topology
       - shader_create_info_t::cull_mode
     */
    void init_as_3d_shader(shader_create_info_t *info, struct render_pipeline_stage_t *stage);
    void init_as_3d_shader_for_stage(shader_create_info_t *info, stage_type_t stage_type);

    // For post processing shaders
    void init_as_render_pipeline_shader(
        const char *vertex_shader_path,
        const char *fragment_shader_path,
        render_pipeline_stage_t *stage,
        VkPipelineLayout layout);
};

/*
  Wrapper for the Vulkan VkBuffer object.
 */
struct gpu_buffer_t {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkDeviceSize size;

    void init(
        uint32_t size,
        void *data,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    void update(
        VkCommandBuffer command_buffer,
        VkPipelineStageFlags pipeline_stage,
        uint32_t offset,
        uint32_t data_size,
        void *data);

    void destroy();
};

/*
  WARNING: Make sure to NEVER call vkDestroyBuffer on a buffer that
  may be being read from in a command buffer that was submitted during
  the previous frame. This will cause a whole lot of nastiness so make
  sure to call 'destroy_sensitive_buffer()' on it.
  
  This will push the push onto a queue of buffers to get deleted. Only after
  a certain amount of frames, will the buffer really get deleted.
 */
void destroy_sensitive_buffer(gpu_buffer_t *buf);

VkDescriptorSet create_buffer_descriptor_set(VkBuffer buffer, VkDeviceSize buffer_size, VkDescriptorType type);

/*
  Each mesh has certain buffers it can have.
 */
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

struct mesh_attrib_t {
    void *data;
    uint32_t size;
    buffer_type_t type;
};

struct mesh_loader_t {
    mesh_attrib_t attribs[MAX_MESH_BUFFERS];
    uint32_t attrib_count;
    buffer_type_t buffer_type_stack[BT_MAX_VALUE];

    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t index_offset;
    VkIndexType index_type;

    // Load vertices, indices, etc...
    void load_sphere();
    void load_cube();
    void load_external(const char *path);

    void push_attrib(buffer_type_t buffer_type, void *p, uint32_t size);
    bool has_attrib(buffer_type_t buffer_type) const;
    mesh_attrib_t *get_attrib(buffer_type_t buffer_type);
    const mesh_attrib_t *get_attrib(buffer_type_t buffer_type) const;
};

// This is the thing we actually need
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
    void init_mesh_vbo_final_list();
    shader_binding_info_t create_shader_binding_info();

    void load(mesh_loader_t *loader, shader_binding_info_t *dst_bindings);
};

/*
  An oddly specific function for the game's rendering API.
  It is simply here because in order have the cool transition
  effect between standing mode and rolling mode, I needed to do
  some fancy trickery (needed a bit more control over what was going on).
  It would have been to complicated and unnecessary to create
  wrapers which could allow that sort of control.
 */
#if 0
void create_player_merged_mesh(
    mesh_t *dst_a, shader_binding_info_t *sbi_a,
    mesh_t *dst_b, shader_binding_info_t *sbi_b,
    mesh_t *dst_merged, shader_binding_info_t *sbi_merged);
#endif

struct merged_mesh_info_t {
    // Skeletal mesh
    mesh_t *dst_sk;
    const mesh_loader_t *sk_loader;
    shader_binding_info_t *dst_sk_sbi;

    // Static mesh
    mesh_t *dst_st;
    const mesh_loader_t *st_loader;
    shader_binding_info_t *dst_st_sbi;

    mesh_t *dst_merged;
    shader_binding_info_t *dst_merged_sbi;
};

void create_merged_mesh(merged_mesh_info_t *info);

/* 
  By default, a uniform buffer is added for camera transforms
  If STATIC is chosen, no extra descriptors will be added to shader information
  If ANIMATED is chosen, 1 extra uniform buffer will be added for joint transforms
  If PASS_EXTRA_UNIFORM_BUFFER, all uniforms will be added from previous options, and
  Extra ones will be added that you pass in the extra descriptors array
*/
enum mesh_type_t {
    MT_STATIC = 1 << 0,
    MT_ANIMATED = 1 << 1,
    MT_PASS_EXTRA_UNIFORM_BUFFER = 1 << 2,
    MT_MERGED_MESH = 1 << 3
};

typedef int32_t mesh_type_flags_t;

/* 
   Need to set:
   - shader_create_info_t::binding_info
   - shader_create_info_t::shader_paths
   - shader_create_info_t::shader_flags
   - shader_create_info_t::cull_mode
   - shader_create_info_t::topology
*/
shader_t create_mesh_shader_color(shader_create_info_t *info, mesh_type_flags_t type);

/* 
   Need to set:
   - shader_create_info_t::binding_info
   - shader_create_info_t::shader_paths
   - shader_create_info_t::shader_flags
   - shader_create_info_t::topology
*/
shader_t create_mesh_shader_shadow(shader_create_info_t *info, mesh_type_flags_t mesh_type);

/*
  Data gets sent through push constant to the shaders
 */
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

/*
  Set of functions to submit mesh objects for rendering
 */
void begin_mesh_submission(VkCommandBuffer cmdbuf, shader_t *shader, VkDescriptorSet extra_set = VK_NULL_HANDLE);
void submit_mesh(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data);
void submit_mesh_shadow(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data);
void submit_skeletal_mesh(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    struct animated_instance_t *instance);
void submit_skeletal_mesh_shadow(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance);

/* 
  Make sure to update the data that is on the GPU with the new data
  (e.g. new camera transforms, etc...) 
*/
void gpu_sync_scene3d_data(VkCommandBuffer cmdbuf, eye_3d_info_t *eye, lighting_info_t *light);

/* 
  Make sure to wrap all shadow rendering between a begin and end shadow rendering
 */
void begin_shadow_rendering(VkCommandBuffer cmdbuf);
void end_shadow_rendering(VkCommandBuffer cmdbuf);

/*
  Make sure to wrap all scene rendering between a begin and end scene rendering
 */
void begin_scene_rendering(VkCommandBuffer cmdbuf);
void end_scene_rendering(VkCommandBuffer cmdbuf);

/*
  Make sure this gets called after 'end_scene_rendering()'
 */
void post_process_scene(struct frame_info_t *info, VkCommandBuffer ui_cmdbuf);

/*
  A crucial part of an animated instance: defines bone relationships.
 */
struct skeleton_t {
    uint32_t joint_count;
    struct joint_t *joints;

    void load(const char *path);
};

/*
  Holds together a set of animation cycles a certain animated instance
  will be able to play through.
 */
struct animation_cycles_t {
    uint32_t cycle_count;
    struct animation_cycle_t *cycles;

    // This also holds some information on how to render this thing
    matrix4_t scale;
    matrix4_t rotation;

    // The linker defines relationship between animations names and IDs
    void load(const char *linker_path, const char *path);
};

/*
  Every animated object will need an animated_instance_t
  which will contain the respective animated bone transforms
  at a certain keyframe.
 */
struct animated_instance_t {
    float current_animation_time;
    float in_between_interpolation_time = 0.1f;
    bool is_interpolating_between_cycles;

    struct skeleton_t *skeleton;
    uint32_t prev_bound_cycle;
    uint32_t next_bound_cycle;
    struct animation_cycles_t *cycles;

    matrix4_t *interpolated_transforms;

    gpu_buffer_t interpolated_transforms_ubo;
    VkDescriptorSet descriptor_set;

    uint32_t *current_position_indices;
    vector3_t *current_positions;
    uint32_t *current_rotation_indices;
    quaternion_t *current_rotations;
    uint32_t *current_scale_indices;
    vector3_t *current_scales;

    void init(struct skeleton_t *skeleton, struct animation_cycles_t *cycles);
    void interpolate_joints(float dt, bool repeat);
    void switch_to_cycle(uint32_t cycle_index, bool force);
    void sync_gpu_with_transforms(VkCommandBuffer cmdbuf);
private:
    void calculate_in_between_final_offsets(uint32_t joint_index, matrix4_t parent_transforms);
    void calculate_in_between_bone_space_transforms(float progression, struct animation_cycle_t *next);
    void calculate_bone_space_transforms(struct animation_cycle_t *bound_cycle, float current_time);
    void calculate_final_offset(uint32_t joint_index, matrix4_t parent_transform);
};

/*
  Simply renders the environment cubemap.
 */
void render_environment(VkCommandBuffer cmdbuf);

/*
  Brightness value between 0 and 1.
  Simply sets the brightness of the screen. (affects the final render pass)
*/
void set_main_screen_brightness(float brightness);

/*
  Wrapper for textures.
 */
struct texture_t {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory image_memory;
    VkSampler sampler;
    VkFormat format;

    VkDescriptorSet descriptor;

    void init(
        const char *path,
        VkFormat format,
        void *data,
        uint32_t width, uint32_t height,
        VkFilter filter);
};

/*
  For debugging with Renderdoc.
 */
void set_object_debug_name(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    const char* name);

void set_object_debug_tag(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    uint64_t name,
    uint32_t tag_size,
    const void *tag);

void begin_debug_region(
    VkCommandBuffer cmdbuf,
    const char *marker_name,
    const vector4_t &color);

void insert_debug(
    VkCommandBuffer cmdbuf,
    const char *name,
    const vector4_t &color);

void end_debug_region(VkCommandBuffer cmdbuf);

}
