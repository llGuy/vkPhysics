#pragma once

#include "cl_main.hpp"
#include <common/event.hpp>
#include <renderer/renderer.hpp>

void world_init(event_submissions_t *events);
void destory_world();
void handle_world_input(highlevel_focus_t focus);
void tick_world(event_submissions_t *events);

void gpu_sync_world(
    bool in_startup,
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer render_shadow_command_buffer,
    VkCommandBuffer transfer_command_buffer);

eye_3d_info_t create_eye_info();
lighting_info_t create_lighting_info();

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

void read_startup_screen();
void write_startup_screen();

void kill_local_player(event_submissions_t *events);
