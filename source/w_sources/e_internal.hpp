#pragma once

#include "engine.hpp"
#include <common/event.hpp>
#include <renderer/renderer.hpp>

struct engine_t {
    bool running = 0;

    event_submissions_t events;
    listener_t game_core_listener;

    highlevel_focus_t focus;

    frame_info_t frame_info;

    game_flags_t master_flags;

    uint64_t current_tick;

    float ldelta_time = 0.0f;

    uint32_t init_flags;
};

void e_fader_init();

void e_begin_fade_effect(
    struct event_begin_fade_effect_t *data);   // destination value should be 1 if fade in or 0 if fade out

void e_tick_fade_effect(
    struct event_submissions_t *events);

void e_event_listener_init();

void e_subscribe_to_events(
    listener_t game_core_listener,
    event_submissions_t *events);

void e_game_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events);

void e_terminate();
void e_begin_startup_world_rendering();
void e_end_startup_world_rendering();
void e_enter_ui_screen();
void e_enter_world();
void e_begin_blurred_rendering();
void e_begin_crisp_rendering();

#define MAX_SECONDARY_COMMAND_BUFFERS 3

struct engine_current_frame_t {
    VkCommandBuffer render_command_buffer;
    VkCommandBuffer render_shadow_command_buffer;
    VkCommandBuffer transfer_command_buffer;
    VkCommandBuffer ui_command_buffer;
};

struct engine_rendering_t {
    uint32_t command_buffer_index = 0;

    uint32_t secondary_command_buffer_count;
    VkCommandBuffer render_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
    VkCommandBuffer render_shadow_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
    VkCommandBuffer transfer_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
    VkCommandBuffer ui_command_buffers[MAX_SECONDARY_COMMAND_BUFFERS];
};

void e_command_buffers_init();

engine_current_frame_t e_prepare_frame();

void e_render(
    engine_t *engine);

void e_finish_frame(
    engine_t *engine);

void e_register_debug_window_proc(
    engine_t *engine);

#if LINK_AGAINST_RENDERER
void e_debug_window();
#endif
