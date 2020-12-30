#pragma once

#include <vk.hpp>
#include <vkph_state.hpp>
#include <vkph_events.hpp>

struct frame_command_buffers_t;

namespace ux {

typedef void(* scene_tick_proc_t)(
    frame_command_buffers_t *,
    vkph::state_t *state);

typedef void(* scene_event_handle_proc_t)(
    void *object,
    vkph::event_t *events);

/*
  The listener object will get passed on so that each scene can subscribe to their
  events.
 */
vkph::listener_t prepare_scene_event_subscription(vkph::state_t *);

/*
  When changing scenes, simply call this.
 */
void bind_scene_procs(
    scene_tick_proc_t,
    scene_event_handle_proc_t);

/*
  Calls the bound tick function.
 */
void tick_scene(frame_command_buffers_t *, vkph::state_t *);

/*
  Every scene needs to have an eye info and a lighting info because the end of every
  frame update, a lighting and eye info needs to get sent to the renderer's end_frame().
 */
struct scene_info_t {
    vk::eye_3d_info_t eye;
    vk::lighting_info_t lighting;
};

scene_info_t *get_scene_info();

}
