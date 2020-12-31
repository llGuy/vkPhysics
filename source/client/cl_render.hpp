#pragma once

#include <vk.hpp>
#include "cl_frame.hpp"
#include <vkph_state.hpp>
#include <vkph_chunk.hpp>

namespace cl {

/*
  All the objects in the game have both a albedo and shadow shader.
 */
struct object_shaders_t {
    vk::shader_t albedo;
    vk::shader_t shadow;
};

/*
  Performs a GPU sync for all the data (buffer updates, etc...) and
  actually renders the game.
 */
void draw_game(frame_command_buffers_t *cmdbufs, vkph::state_t *state);



/*
  CHUNK RENDERING:
 */
void init_chunk_render_resources();

void chunks_gpu_sync_and_render(
    struct frustum_t *frustum,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state);

using chunk_render_data_t = vk::mesh_render_data_t;

/*
  We do math and calculate the chunk mesh vertices with this structure.
 */
struct uncompressed_chunk_mesh_vertex_t {
    vector3_t position;
    uint32_t color;
};

/*
  We actually send this to the GPU as vertex attributes.
 */
struct compressed_chunk_mesh_vertex_t {
    /* 
      12 bits for the coordinate of the nearest voxel
      20 bits: 8 (normalized x of the vertex), 8 (normalized y of the vertex), 4 (first 4 bits of z)
    */
    uint32_t low;
    // 4 bits for the second half of z, and 8 bits for the color
    uint32_t high;
    // Find way to avoid wasting 20 bits !
};

/*
  Struct that was forward declared in vkph_chunk.hpp
 */
struct chunk_render_t {
    vk::mesh_t mesh;
    chunk_render_data_t render_data;
};

chunk_render_t *init_chunk_render(const vkph::chunk_t *c, const vector3_t &ws_position);
void destroy_chunk_render(chunk_render_t *render);

/*
  Calls generate chunk verts and updates the vertex buffer.
 */
void update_chunk_gpu_resources(
    VkCommandBuffer cmdbuf,
    uint8_t surface_level,
    vkph::chunk_t *c,
    compressed_chunk_mesh_vertex_t *verts,
    const vkph::state_t *state);



/*
  PLAYER RENDERING:
 */
void init_player_render_resources();

void players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    vkph::state_t *state);

using player_render_data_t = vk::mesh_render_data_t;

struct player_render_t {
    player_render_data_t render_data;
    vk::animated_instance_t animations;
    float rotation_speed;
    float rotation_angle;
    matrix4_t rolling_matrix;
    float shape_animation_time;

    struct {
        uint8_t repeat: 1;
    };
};

player_render_t *init_player_render();
void init_player_animated_instance(vk::animated_instance_t *instance);

/* 
  Helper function which for a gien animated state, says if it's a repeating animation,
  or one that will stop after the animation cycle reaches the end of its timeline.
 */
bool is_animation_repeating(vkph::player_animated_state_t state);

bool &is_first_person();



/*
  PROJECTILES RENDERING:
 */
void init_projectile_render_resources();

void projectiles_gpu_sync_and_render(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    VkCommandBuffer transfer,
    vkph::state_t *state);



/*
  PREMADE SCENE RENDERING:
  
  NOTE: Premade scenes are scenes for which we don't need to generate the chunk information
  to render. For instance, one of them is used for the main menu screen.
 */

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

    void load(const char *path);
};

void premade_scene_gpu_sync_and_render(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    fixed_premade_scene_t *scene);

}
