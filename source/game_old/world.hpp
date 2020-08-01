#pragma once

#include "game/engine.hpp"
#include <common/math.hpp>
#include <common/time.hpp>
#include <common/event.hpp>
#include <renderer/renderer.hpp>
#include <common/containers.hpp>

#define PLAYER_WALKING_SPEED 25.0f

void world_init(
    event_submissions_t *events);

void destory_world();

void handle_world_input(
    highlevel_focus_t focus);

void tick_world(
    event_submissions_t *events);

void gpu_sync_world(
    bool in_startup,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer);

eye_3d_info_t create_eye_info();
lighting_info_t create_lighting_info();

// Push constant
// chunk_render_data_t may become a different structure in the future.

void activate_chunk_history(
    chunk_t *chunk);

void reset_modification_tracker();

uint32_t get_voxel_index(
    uint32_t x,
    uint32_t y,
    uint32_t z);

void set_chunk_history_tracker_value(
    bool value);

struct chunks_to_interpolate_t {
    float elapsed;
    uint32_t max_modified;
    uint32_t modification_count;
    struct chunk_modifications_t *modifications;
};

chunks_to_interpolate_t *get_chunks_to_interpolate();

// For AI
#define AI_SENSOR_COUNT 26
struct sensors_t {
    float *s;
};

void cast_ray_sensors(
    sensors_t *sensors,
    const vector3_t &ws_position,
    const vector3_t &ws_view_direction,
    const vector3_t &ws_up_vector);

// For debugging only
stack_container_t<player_t *> &DEBUG_get_players();
player_t *DEBUG_get_spectator();

void read_startup_screen();
void write_startup_screen();

void kill_local_player(
    event_submissions_t *events);
