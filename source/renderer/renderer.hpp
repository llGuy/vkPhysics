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

struct shader_t {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkShaderStageFlags flags;
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

DECLARE_VOID_RENDERER_PROC(void, end_frame,
    frame_info_t *frame_info);

// This needs to be created and passed to the renderer module to update camera transforms
// And lighting information
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



// For the data needed to render (vertex offset, first index, index_count, index_offset, index_type) - will be set to default values by load_mesh_* functions

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
