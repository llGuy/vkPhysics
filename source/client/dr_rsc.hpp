#pragma once

#include <vk.hpp>
#include <common/tools.hpp>

// Shaders, meshes, etc...
void dr_resources_init();
void dr_player_animated_instance_init(vk::animated_instance_t *instance);
struct compressed_chunk_mesh_vertex_t *dr_get_tmp_mesh_verts();

struct fixed_premade_scene_t {
    struct {
        uint32_t flags;
        union {
            uint32_t initialised: 1;
        };
    };

    vk::mesh_t world_mesh;
    vk::mesh_render_data_t world_render_data;

    vector3_t position, view_direction, up_vector;
};

fixed_premade_scene_t dr_read_premade_rsc(const char *path);

enum game_mesh_t { GM_PLAYER, GM_BALL, GM_MERGED, GM_BULLET, GM_INVALID };
vk::mesh_t *dr_get_mesh_rsc(game_mesh_t mesh);

enum game_shader_t {
    GS_PLAYER,
    GS_PLAYER_SHADOW,
    GS_BALL,
    GS_BALL_SHADOW,
    GS_MERGED_BALL,
    GS_MERGED_BALL_SHADOW,
    GS_MERGED_PLAYER,
    GS_MERGED_PLAYER_SHADOW,
    GS_CHUNK,
    GS_CHUNK_SHADOW,
    GS_INVALID
};

vk::shader_t *dr_get_shader_rsc(game_shader_t shader);

struct chunk_color_data_t {
    vector4_t pointer_position;
    vector4_t pointer_color;
    float pointer_radius;
};

struct chunk_color_mem_t {
    vk::gpu_buffer_t chunk_color_ubo;
    VkDescriptorSet chunk_color_set;
    chunk_color_data_t chunk_color;
};

extern chunk_color_mem_t dr_chunk_colors_g;
