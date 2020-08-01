#pragma once

#include <common/tools.hpp>
#include <common/event.hpp>
#include <common/chunk.hpp>
#include <common/player.hpp>
#include <renderer/input.hpp>
#include <common/containers.hpp>
#include <renderer/renderer.hpp>

// PLAYER STUFF ///////////////////////////////////////////////////////////////
void w_player_world_init();
void w_clear_players_and_render_rsc();

void w_player_render_init(player_t *player);
void w_handle_input(game_input_t *input, float dt);

void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer);

void w_tick_players(event_submissions_t *events);

int32_t w_local_player_index();
player_t *w_get_local_player();
player_t *w_get_spectator();
stack_container_t<player_t *> &w_get_players();

void w_set_local_player(int32_t local_id);
void w_execute_player_actions(player_t *player, event_submissions_t *events);

// CHUNKS /////////////////////////////////////////////////////////////////////
void w_chunks_data_init();
void w_chunk_world_init(
    uint32_t loaded_radius);

void w_chunk_render_init(
    chunk_t *chunk,
    const vector3_t &ws_position);

void w_tick_chunks(
    float dt);

void w_chunk_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer);

// Would call this at end of game or when chunk is out of chunk load radius.
void w_destroy_chunk_render(
    chunk_t *chunk);

// Returns NULL
// Usage: chunk = w_destroy_chunk(chun)
chunk_t *w_destroy_chunk(
    chunk_t *chunk);

// Clear the chunks and the rendering resources
void w_clear_chunks_and_render_rsc();

void w_destroy_chunk_data();

void w_toggle_mesh_update_wait(
    bool value);

uint8_t w_get_surface_level();

stack_container_t<chunk_t *> &w_get_chunks();

// Returns vertex count
uint32_t w_create_chunk_vertices(
    uint8_t surface_level,
    chunk_t *c,
    vector3_t *mesh_vertices = NULL);

void w_update_chunk_mesh(
    VkCommandBuffer command_buffer,
    uint8_t surface_level,
    chunk_t *c,
    vector3_t *mesh_vertices = NULL);

void w_allocate_temp_vertices_for_chunk_mesh_creation();
void w_free_temp_vertices_for_chunk_mesh_creation();

// COLLISION WITH TERRAIN /////////////////////////////////////////////////////
enum collision_primitive_type_t { CPT_FACE, CPT_EDGE, CPT_VERTEX };

struct collision_triangle_t {
    union {
        struct {
            vector3_t a;
            vector3_t b;
            vector3_t c;
        } v;
        vector3_t vertices[3];
    };
};


// STARTUP SCREEN /////////////////////////////////////////////////////////////
struct startup_screen_t {
    struct {
        uint32_t flags;
        union {
            uint32_t initialised: 1;
        };
    };

    mesh_t world_mesh;
    mesh_render_data_t world_render_data;

    vector3_t position, view_direction, up_vector;
};

void w_startup_init();

startup_screen_t *w_get_startup_screen_data();

void w_render_startup_world(
    VkCommandBuffer render_command_buffer);

void w_read_startup_screen();

void w_write_startup_screen();

vector3_t w_update_spectator_view_direction(
    const vector3_t &spectator_view_direction);

#define MAX_LOADED_CHUNKS 1000
#define MAX_VOXEL_VALUE_F 254.0f
#define MAX_VERTICES_PER_CHUNK 5 * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1) * (CHUNK_EDGE_LENGTH - 1)

void w_begin_ai_training_players(
    ai_training_session_t type);

void w_begin_ai_training_chunks(
    ai_training_session_t type);

void w_reposition_spectator();

void w_handle_spectator_mouse_movement();

terraform_package_t *w_get_local_current_terraform_package();

struct context_t {
    struct {
        uint8_t in_server: 1;
        // Are you in a menu where you aren't occupying a player
        uint8_t in_meta_menu: 1;
        uint8_t in_gameplay: 1;
    };

    listener_t world_listener;
};

void w_subscribe_to_events(
    context_t *context,
    listener_t listener,
    event_submissions_t *events);

void w_world_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events);

struct chunk_color_data_t {
    vector4_t pointer_position;
    vector4_t pointer_color;
    float pointer_radius;
};

struct scene_rendering_t {
    mesh_t player_mesh;
    mesh_t player_ball_mesh;
    mesh_t merged_mesh;

    skeleton_t player_skeleton;
    animation_cycles_t player_cycles;

    shader_t player_shadow_shader;
    shader_t player_shader;
    shader_t player_ball_shader;
    shader_t player_ball_shadow_shader;
    shader_t merged_shader_ball; // Go from ball to person
    shader_t merged_shader_player; // Go from person to ball
    shader_t chunk_shader;

    gpu_buffer_t chunk_color_data_buffer;
    VkDescriptorSet chunk_color_data_buffer_set;

    chunk_color_data_t chunk_color_data;
};

void w_create_shaders_and_meshes();

void w_player_animated_instance_init(
    animated_instance_t *instance);

bool w_animation_is_repeat(
    player_animated_state_t state);

// SOME AI STUFF //////////////////////////////////////////////////////////////
struct world_ai_t {
    struct {
        uint8_t in_training: 1;
        uint8_t first_iteration: 1;
    };
    ai_training_session_t training_type;

    uint32_t iteration_count;
    uint32_t ai_iteration_stride;

    uint32_t population_count;
};

void w_begin_ai_training(
    ai_training_session_t session_type);

void w_finish_generation();

bool w_check_ai_population(float dt);
