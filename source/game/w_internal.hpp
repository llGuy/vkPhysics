#pragma once

#include "world.hpp"
#include <common/tools.hpp>
#include <common/event.hpp>
#include <renderer/input.hpp>
#include <common/containers.hpp>
#include <renderer/renderer.hpp>

// PLAYER STUFF ///////////////////////////////////////////////////////////////
#define MAX_PLAYERS 50

struct players_t {
    int32_t local_player;
    player_t *spectator;
    stack_container_t<player_t *> players;
    // From client id, get player
    int16_t local_id_from_client_id[MAX_PLAYERS];
};

void w_player_world_init();

// Just adds a player to the list of players
player_t *w_add_player();

// Adds a player to the list and initialises it
player_t *w_add_player_from_info(
    player_init_info_t *info);

void w_player_render_init(
    player_t *player);

void w_link_client_id_to_local_id(
    uint16_t client_id,
    uint32_t local_id);

void w_handle_input(
    game_input_t *input,
    float dt);

void w_players_gpu_sync_and_render(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer);

void w_tick_players(
    event_submissions_t *events);

void w_execute_player_actions(
    player_t *player,
    event_submissions_t *events);

int32_t w_local_player_index();
player_t *w_get_local_player();
player_t *w_get_spectator();
vector3_t w_get_player_scale();
stack_container_t<player_t *> &w_get_players();

void w_set_local_player(
    int32_t local_id);

void w_destroy_player(
    uint32_t id);

void w_clear_players();

// CHUNKS /////////////////////////////////////////////////////////////////////
void w_chunks_data_init();
void w_chunk_world_init(
    uint32_t loaded_radius);

void w_chunk_init(
    chunk_t *chunk,
    uint32_t chunk_stack_index,
    const ivector3_t &chunk_coord);

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

void w_clear_chunk_world();

void w_destroy_chunk_data();

// Any function suffixed with _m means that the function will cause chunks to be added to a list needing gpu sync
void w_add_sphere_m(
    const vector3_t &ws_center,
    float ws_radius);

// Pretty useless, as world and voxel coordinates are basically the same
ivector3_t w_convert_world_to_voxel(
    const vector3_t &ws_position);

// Gets the coordinate of the chunk which contains the voxel
ivector3_t w_convert_voxel_to_chunk(
    const ivector3_t &vs_position);

// Converts from the bottom xyz corner of a chunk to world coordinates (given a chunk coordinate)
vector3_t w_convert_chunk_to_world(
    const ivector3_t &chunk_coord);

// Converts from "world" coordinates to the voxel 16x16x16 coordinate of the voxel that the coordinate is in
ivector3_t w_convert_voxel_to_local_chunk(
    const ivector3_t &vs_position);

#define TERRAFORMING_RADIUS 3.0f
#define TERRAFORMING_SPEED 200.0f

enum terraform_type_t { TT_DESTROY, TT_BUILD };

bool w_terraform(
    terraform_type_t type,
    terraform_package_t package,
    float radius,
    float speed,
    float dt);

terraform_package_t w_cast_terrain_ray(
    const vector3_t &ws_ray_start,
    const vector3_t &ws_ray_direction,
    float max_reach);

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

struct terrain_collision_t {
    // Flags
    union {
        struct {
            uint32_t detected: 1;
            uint32_t has_detected_previously: 1;
            uint32_t under_terrain: 1;
            uint32_t recurse: 4;
        };
        uint32_t flags;
    };

    // Data passed between recursive calls
    // All these have to be filled in when calling collide_and_slide
    vector3_t es_position;
    vector3_t es_velocity;

    vector3_t ws_size;
    vector3_t ws_position;
    vector3_t ws_velocity;
    // -------------------------------------------------------------
    
    vector3_t es_normalised_velocity;
    vector3_t es_surface_normal;

    float es_nearest_distance;
    vector3_t es_contact_point;
};

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

vector3_t w_collide_and_slide(
    terrain_collision_t *collision);

vector3_t w_test_collision(
    terrain_collision_t *collision,
    collision_triangle_t *triangle);

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
