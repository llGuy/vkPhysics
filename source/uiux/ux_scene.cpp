#include "ux_scene.hpp"
#include "vkph_events.hpp"

namespace ux {

static vkph::listener_t listener;
static scene_tick_proc_t tick;
static scene_event_handle_proc_t handle_event;
static scene_info_t scene_info;

static void s_event_listener(void *object, vkph::event_t *event) {
    assert(handle_event);
    handle_event(object, event);
}

vkph::listener_t prepare_scene_event_subscription(vkph::state_t *state) {
    listener = vkph::set_listener_callback(s_event_listener, state);
    return listener;
}

void bind_scene_procs(scene_tick_proc_t tick_proc, scene_event_handle_proc_t event_proc) {
    tick = tick_proc;
    handle_event = event_proc;
}

void tick_scene(frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    assert(tick);
    tick(cmdbufs, state);
}

scene_info_t *get_scene_info() {
    return &scene_info;
}

}
