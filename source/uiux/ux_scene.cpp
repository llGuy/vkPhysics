#include "ux_scene.hpp"
#include "vkph_events.hpp"

namespace ux {

static constexpr uint32_t MAX_SCENE_COUNT = 10;

static vkph::listener_t listener;
static uint32_t scene_count = 0;
static scene_t *scenes[MAX_SCENE_COUNT];
static uint32_t bound_scene;
static scene_info_t scene_info;

uint32_t push_scene(scene_t *scene) {
    scenes[scene_count] = scene;
    return scene_count++;
}

void bind_scene(uint32_t id, vkph::state_t *state) {
    scenes[bound_scene]->prepare_for_unbinding(state);

    bound_scene = id;

    scenes[bound_scene]->prepare_for_binding(state);
}

static void s_event_listener(void *object, vkph::event_t *event) {
    scenes[bound_scene]->handle_event(object, event);
}

void init_scenes(vkph::state_t *state) {
    listener = vkph::set_listener_callback(s_event_listener, state);

    for (uint32_t i = 0; i < scene_count; ++i) {
        scenes[i]->init();
        scenes[i]->subscribe_to_events(listener);
    }
}

void tick_scene(cl::frame_command_buffers_t *cmdbufs, vkph::state_t *state) {
    scenes[bound_scene]->tick(cmdbufs, state);
}

scene_info_t *get_scene_info() {
    return &scene_info;
}

}
