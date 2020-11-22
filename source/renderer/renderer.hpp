#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <common/math.hpp>
#include <common/tools.hpp>
#include <vulkan/vulkan_core.h>

typedef void(*surface_proc_t)(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window);

// (For server application - just console application), we aren't linking with the renderer static library
// So there won't be any binary definitions for the renderer's functions.
// If we aren't linking against renderer, we need to give all the functions an inline definition
// That will just be an empty body, hence these macros.
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



// CORE VULKAN FUNCTIONS //////////////////////////////////////////////////////
DECLARE_VOID_RENDERER_PROC(void, renderer_init, void);

DECLARE_VOID_RENDERER_PROC(void, destroy_renderer,
    void);

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

DECLARE_VOID_RENDERER_PROC(void, create_command_buffers,
    VkCommandBufferLevel level, 
    VkCommandBuffer *command_buffers, uint32_t count);

enum render_pass_inheritance_t {
    RPI_SHADOW, RPI_DEFERRED, RPI_POST_PROCESS, RPI_UI
};

DECLARE_VOID_RENDERER_PROC(void, fill_main_inheritance_info,
    VkCommandBufferInheritanceInfo *info,
    render_pass_inheritance_t inheritance_type);

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

DECLARE_RENDERER_PROC(void *, map_gpu_memory,
    VkDeviceMemory memory,
    uint32_t size,
    uint32_t offset);

DECLARE_VOID_RENDERER_PROC(void, unmap_gpu_memory,
    VkDeviceMemory memory);

struct gpu_buffer_t {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkDeviceSize size;
};

DECLARE_RENDERER_PROC(gpu_buffer_t, create_gpu_buffer,
    uint32_t size,
    void *data,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

struct shader_binding_info_t {
    uint32_t binding_count;
    VkVertexInputBindingDescription *binding_descriptions;

    uint32_t attribute_count;
    VkVertexInputAttributeDescription *attribute_descriptions;
};

struct shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkShaderStageFlags flags;
};

enum alpha_blending_t {
    AB_ONE_MINUS_SRC_ALPHA,
    AB_NONE
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
    VkPrimitiveTopology topology,
    alpha_blending_t blending = AB_NONE);

// Enables depth and will always happen in deferred stage
DECLARE_RENDERER_PROC(shader_t, create_3d_shader_shadow,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size, VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkPrimitiveTopology topology,
    VkShaderStageFlags shader_flags);

// NOTE: Very important
// FOR 3D MESHES (NOT ANIMATED) THE FIRST DESCRIPTOR TYPE NEEDS TO BE UNIFORM BUFFER
// FOR CAMERA TRANSFORMS UNIFORM BUFFER WHICH WILL PASSED BY DEFAULT FOR SUBMIT_MESH FUNCTION
// FOR SKELETAL ANIMATIONS, THE SECOND NEEDS TO BE UNIFORM BUFFER AS WELL BECAUSE OF SKELETAL
// JOINT TRANSFORMS. THIS WILL BE PASSED TO SUBMIT_SKELETAL_MESH FUNCTION.
DECLARE_RENDERER_PROC(shader_t, create_3d_shader_color,
    shader_binding_info_t *binding_info,
    uint32_t push_constant_size,
    VkDescriptorType *descriptor_layout_types,
    uint32_t descriptor_layout_count,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    VkCullModeFlags culling);

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



// RENDERING FUNCTIONALITY ////////////////////////////////////////////////////
DECLARE_RENDERER_PROC(VkCommandBuffer, begin_frame, void);

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

DECLARE_VOID_RENDERER_PROC(void, end_frame,
    frame_info_t *frame_info);

// This needs to be created and passed to the renderer module to update camera transforms
// And lighting information
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

DECLARE_VOID_RENDERER_PROC(void, gpu_data_sync,
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye,
    lighting_info_t *lighting);

DECLARE_VOID_RENDERER_PROC(void, render_environment,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, post_process_scene,
    frame_info_t *frame_info,
    VkCommandBuffer ui_command_buffer);

DECLARE_VOID_RENDERER_PROC(void, begin_scene_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, end_scene_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, begin_shadow_rendering,
    VkCommandBuffer command_buffer);

DECLARE_VOID_RENDERER_PROC(void, end_shadow_rendering,
    VkCommandBuffer command_buffer);



// MESH AND ANIMATIONS ////////////////////////////////////////////////////////
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

DECLARE_VOID_RENDERER_PROC(void, load_mesh_internal,
    internal_mesh_type_t mesh_type,
    mesh_t *mesh,
    shader_binding_info_t *info);

DECLARE_VOID_RENDERER_PROC(void, load_mesh_external,
    mesh_t *mesh,
    shader_binding_info_t *info,
    const char *path);

DECLARE_VOID_RENDERER_PROC(void, create_player_merged_mesh,
    mesh_t *dst_a, shader_binding_info_t *sbi_a,
    mesh_t *dst_b, shader_binding_info_t *sbi_b,
    mesh_t *dst_merged, shader_binding_info_t *sbi_merged);

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

    // This also holds some information on how to render this thing
    matrix4_t scale;
    matrix4_t rotation;
};

DECLARE_VOID_RENDERER_PROC(void, load_animation_cycles,
    animation_cycles_t *cycles,
    const char *linker_path, // Defines relationship between bone and index + blending between animations (priorities)
    const char *path);

struct animated_instance_t {
    float current_animation_time;
    float in_between_interpolation_time = 0.1f;
    bool is_interpolating_between_cycles;

    skeleton_t *skeleton;
    uint32_t prev_bound_cycle;
    uint32_t next_bound_cycle;
    animation_cycles_t *cycles;

    matrix4_t *interpolated_transforms;

    gpu_buffer_t interpolated_transforms_ubo;
    VkDescriptorSet descriptor_set;

    uint32_t *current_position_indices;
    vector3_t *current_positions;
    uint32_t *current_rotation_indices;
    quaternion_t *current_rotations;
    uint32_t *current_scale_indices;
    vector3_t *current_scales;
};

DECLARE_VOID_RENDERER_PROC(void, animated_instance_init,
    animated_instance_t *instance,
    skeleton_t *skeleton,
    animation_cycles_t *cycles);

// TODO: Create new animation system with this instead of the is_interpolating_between_cycles thing
struct joint_interpolation_t {
    struct {
        // If is_current is set, then this animation refers to the currently bound cycle
        // If not, we need to choose the one at the specified index
        uint8_t is_current: 1;
        uint8_t index: 7;
    };

    float priority;
};

struct mixed_joint_interpolation_recipe_t {
    uint32_t mixing_count;
    joint_interpolation_t *mixing_list;
};

DECLARE_VOID_RENDERER_PROC(void, interpolate_joints_mixed,
    animated_instance_t *instance,
    float dt);

DECLARE_VOID_RENDERER_PROC(void, interpolate_joints,
    animated_instance_t *instance,
    float dt,
    bool repeat);

DECLARE_VOID_RENDERER_PROC(void, switch_to_cycle,
    animated_instance_t *instance,
    uint32_t cycle_index,
    bool force);

DECLARE_VOID_RENDERER_PROC(void, sync_gpu_with_animated_transforms,
    animated_instance_t *instance,
    VkCommandBuffer command_buffer);

struct mesh_render_data_t {
    matrix4_t model;
    vector4_t color;

    // .x = roughness
    // .y = metallic
    vector4_t pbr_info;
    
    // To add later with texture stuff
    int32_t texture_index;
};

#define DEF_MESH_RENDER_DATA_SIZE (sizeof(mesh_render_data_t))

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

DECLARE_RENDERER_PROC(shader_t, create_mesh_shader_color,
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling,
    VkPrimitiveTopology topology,
    mesh_type_flags_t type);

DECLARE_RENDERER_PROC(shader_t, create_mesh_shader_shadow,
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    mesh_type_flags_t type);

DECLARE_RENDERER_PROC(shader_t, create_mesh_shader_alpha,
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    mesh_type_flags_t type);

DECLARE_RENDERER_PROC(shader_binding_info_t, create_mesh_binding_info,
    mesh_t *mesh);

DECLARE_VOID_RENDERER_PROC(void, free_mesh_binding_info,
    shader_binding_info_t *);

DECLARE_VOID_RENDERER_PROC(void, begin_mesh_submission,
    VkCommandBuffer command_buffer,
    shader_t *shader,
    VkDescriptorSet extra_set = VK_NULL_HANDLE);

// Submits only one mesh
DECLARE_VOID_RENDERER_PROC(void, submit_mesh,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data);

DECLARE_VOID_RENDERER_PROC(void, submit_mesh_shadow,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data);

// Submits skeletal mesh
DECLARE_VOID_RENDERER_PROC(void, submit_skeletal_mesh,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance);

DECLARE_VOID_RENDERER_PROC(void, submit_skeletal_mesh_shadow,
    VkCommandBuffer command_buffer,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance);



// DEBUGGIN ///////////////////////////////////////////////////////////////////
typedef void (*debug_ui_proc_t)();
DECLARE_VOID_RENDERER_PROC(void, add_debug_ui_proc,
    debug_ui_proc_t proc);



// USER INTERFACE /////////////////////////////////////////////////////////////
enum coordinate_type_t { PIXEL, GLSL };

struct ui_vector2_t {
    union {
        struct {int32_t ix, iy;};
        struct {float fx, fy;};
    };
    coordinate_type_t type;

    ui_vector2_t(void) = default;
    ui_vector2_t(float x, float y) : fx(x), fy(y), type(GLSL) {}
    ui_vector2_t(int32_t x, int32_t y) : ix(x), iy(y), type(PIXEL) {}
    ui_vector2_t(const ivector2_t &iv) : ix(iv.x), iy(iv.y) {}
    
    inline vector2_t to_fvec2(void) const { return vector2_t(fx, fy); }
    inline ivector2_t to_ivec2(void) const { return ivector2_t(ix, iy); }
};

enum relative_to_t {
    RT_LEFT_DOWN,
    RT_LEFT_UP,
    RT_RELATIVE_CENTER,
    RT_RIGHT_DOWN,
    RT_RIGHT_UP,
    RT_CENTER,
};

// Math functions
DECLARE_RENDERER_PROC(vector2_t, convert_glsl_to_normalized,
    const vector2_t &position);

DECLARE_RENDERER_PROC(vector2_t, convert_normalized_to_glsl,
    const vector2_t &position);

DECLARE_RENDERER_PROC(ui_vector2_t, glsl_to_pixel_coord,
    const ui_vector2_t &position,
    const VkExtent2D &resolution);

DECLARE_RENDERER_PROC(vector2_t, glsl_to_pixel_coord,
    const vector2_t &position,
    const VkExtent2D &resolution);

DECLARE_RENDERER_PROC(ui_vector2_t, pixel_to_glsl_coord,
    const ui_vector2_t &position,
    const VkExtent2D &resolution);

DECLARE_RENDERER_PROC(vector2_t, pixel_to_glsl_coord,
    const vector2_t &position,
    const VkExtent2D &resolution);

DECLARE_RENDERER_PROC(vector2_t, convert_pixel_to_ndc,
    const vector2_t &pixel_coord);

DECLARE_RENDERER_PROC(uint32_t, vec4_color_to_ui32b,
    const vector4_t &color);

DECLARE_RENDERER_PROC(vector4_t, ui32b_color_to_vec4,
    uint32_t color);

struct gui_colored_vertex_t {
    vector2_t position;
    uint32_t color;
};

struct gui_textured_vertex_t {
    vector2_t position;
    vector2_t uvs;
    uint32_t color;
};

#define UI_BOX_SPECIAL_RESOLUTION 0xFFFFFFFF

struct ui_box_t {
    ui_box_t *parent {nullptr};
    relative_to_t relative_to;
    ui_vector2_t relative_position;
    ui_vector2_t gls_position;
    ui_vector2_t px_position;
    ui_vector2_t gls_max_values;
    ui_vector2_t px_current_size;
    ui_vector2_t gls_current_size;
    ui_vector2_t gls_relative_size;
    float aspect_ratio;
    uint32_t color;

    DECLARE_VOID_RENDERER_PROC(void, init,
        relative_to_t in_relative_to,
        float in_aspect_ratio,
        ui_vector2_t position /* coord_t space agnostic */,
        ui_vector2_t in_gls_max_values /* max_t X and Y size */,
        ui_box_t *in_parent,
        uint32_t in_color,
        VkExtent2D backbuffer_resolution = VkExtent2D{ UI_BOX_SPECIAL_RESOLUTION, UI_BOX_SPECIAL_RESOLUTION });
    
    DECLARE_VOID_RENDERER_PROC(void, update_size,
        const VkExtent2D &backbuffer_resolution);

    DECLARE_VOID_RENDERER_PROC(void, update_position,
        const VkExtent2D &backbuffer_resolution);

    DECLARE_VOID_RENDERER_PROC(void, rotate_clockwise,
        float rad_angle);
};

#define MAX_TEXTURES_IN_RENDER_LIST 25
#define MAX_QUADS_TO_RENDER 1000

DECLARE_VOID_RENDERER_PROC(void, ui_rendering_init,
    void);

DECLARE_VOID_RENDERER_PROC(void, mark_ui_textured_section,
    VkDescriptorSet group);

DECLARE_VOID_RENDERER_PROC(void, push_textured_vertex,
    const gui_textured_vertex_t &vertex);

DECLARE_VOID_RENDERER_PROC(void, push_textured_ui_box,
    const ui_box_t *box);

DECLARE_VOID_RENDERER_PROC(void, push_textured_ui_box,
    const ui_box_t *box,
    vector2_t *uvs); // Needs 6 uvs

DECLARE_VOID_RENDERER_PROC(void, clear_texture_list_containers,
    void);

DECLARE_VOID_RENDERER_PROC(void, sync_gpu_with_textured_vertex_list,
    VkCommandBuffer command_buffer); 
    
DECLARE_VOID_RENDERER_PROC(void, render_textured_quads,
    VkCommandBuffer command_buffer); 

DECLARE_VOID_RENDERER_PROC(void, push_vertex,
    const gui_colored_vertex_t &vertex); 

DECLARE_VOID_RENDERER_PROC(void, push_colored_ui_box,
    const ui_box_t *box);

DECLARE_VOID_RENDERER_PROC(void, push_reversed_colored_ui_box,
    const ui_box_t *box,
    const vector2_t &size);

DECLARE_VOID_RENDERER_PROC(void, push_ui_text,
    struct ui_text_t *text,
    bool secret = 0);

DECLARE_VOID_RENDERER_PROC(void, push_ui_input_text,
    bool render_cursor,
    bool secret,
    uint32_t cursor_color,
    struct ui_input_text_t *text);
    
DECLARE_VOID_RENDERER_PROC(void, clear_ui_color_containers,
    void);

DECLARE_VOID_RENDERER_PROC(void, sync_gpu_with_vertex_list,
    VkCommandBuffer command_buffer); 

DECLARE_VOID_RENDERER_PROC(void, render_colored_quads,
    VkCommandBuffer command_buffer); 

DECLARE_VOID_RENDERER_PROC(void, render_submitted_ui,
    VkCommandBuffer transfer_command_buffer,
    VkCommandBuffer ui_command_buffer);

struct font_character_t {
    char character_value;
    vector2_t uvs_base;
    vector2_t uvs_size;
    vector2_t display_size;
    vector2_t offset;
    float advance;
};

struct font_t {
    uint32_t char_count;
    texture_t font_img;

    font_character_t font_characters[0xFF];
};

struct ui_text_t {
    ui_box_t *dst_box;
    struct font_t *font;
    
    // Max characters = 500
    uint32_t colors[500] = {};
    char characters[500] = {};
    uint32_t char_count = 0;

    enum font_stream_box_relative_to_t { TOP, BOTTOM, CENTER };

    font_stream_box_relative_to_t relative_to;

    // Relative to xadvance
    float x_start;
    float y_start;

    uint32_t chars_per_line;

    // Relative to xadvance
    float line_height;

    DECLARE_VOID_RENDERER_PROC(void, init,
        ui_box_t *box,
        font_t *font,
        font_stream_box_relative_to_t relative_to,
        float px_x_start,
        float px_y_start,
        uint32_t chars_per_line,
        float line_height);

    DECLARE_VOID_RENDERER_PROC(void, draw_char,
        char character,
        uint32_t color);

    DECLARE_VOID_RENDERER_PROC(void, draw_string,
        const char *string,
        uint32_t color);

    DECLARE_VOID_RENDERER_PROC(void, null_terminate,
        void);
};

struct ui_input_text_t {
    uint32_t cursor_position;
    ui_text_t text;

    uint32_t cursor_fade;

    uint32_t text_color;
    bool fade_in_or_out = 0;

    DECLARE_VOID_RENDERER_PROC(void, input,
        struct raw_input_t *raw_input);

    DECLARE_POINTER_RENDERER_PROC(const char *, get_string,
        void);
};

DECLARE_POINTER_RENDERER_PROC(font_t *, load_font,
    const char *fnt_file,
    const char *png_file);

// FADE EFFECT SUPPORT ////////////////////////////////////////////////////////
DECLARE_VOID_RENDERER_PROC(void, set_main_screen_brightness,
    float brightness);

enum frustum_corner_t {
    FLT, FLB,
    FRT, FRB,
    NLT, NLB,
    NRT, NRB
};    

enum frustum_plane_t {
    NEAR, FAR,
    LEFT, RIGHT,
    TOP, BOTTOM
};

struct frustum_t {
    vector3_t vertex[8];
    plane_t planes[6];

    vector3_t position;
    vector3_t direction;
    vector3_t up;
    // Radians
    float fov;
    float aspect;
    float near;
    float far;
};

// FOV needs to be in radians
DECLARE_VOID_RENDERER_PROC(void, create_frustum,
    frustum_t *frustum,
    const vector3_t &p,
    const vector3_t &d,
    const vector3_t &u,
    float fov,
    float aspect,
    float near,
    float far);

DECLARE_RENDERER_PROC(bool, frustum_check_point,
    frustum_t *frustum,
    const vector3_t &point);

DECLARE_RENDERER_PROC(bool, frustum_check_cube,
    frustum_t *frustum,
    const vector3_t &center,
    float radius);
