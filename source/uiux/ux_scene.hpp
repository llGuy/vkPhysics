#pragma once

#include <vk.hpp>
#include <vkph_state.hpp>
#include <vkph_events.hpp>

struct frame_command_buffers_t;

namespace ux {

struct scene_t {
    virtual void init() = 0;
    virtual void subscribe_to_events(vkph::listener_t listener) = 0;
    virtual void tick(frame_command_buffers_t *cmdbufs, vkph::state_t *state) = 0;
    virtual void handle_event(void *object, vkph::event_t *events) = 0;
    virtual void prepare_for_binding(vkph::state_t *state) = 0;
    virtual void prepare_for_unbinding(vkph::state_t *state) = 0;
};

/*
  Before the game begins, client will need to push all the possible scenes
  with this function. The order of pushing matters and needs to correlate with
  the ID that the client will use to refer to them when binding.
  Returns the scene ID.
 */
uint32_t push_scene(scene_t *scene);

/*
  When changing scenes, simply call this.
  This will call the prepare_for_unbinding function of the currently bound scene
  and then the prepare_for_binding function of the newly bound scene.
 */
void bind_scene(uint32_t id, vkph::state_t *state);

/*
  Calls the init and subscribe_to_events functions of all the pushed scenes.
 */
void init_scene(vkph::state_t *);

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
