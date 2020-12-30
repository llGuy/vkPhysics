#include "sc_scene.hpp"
#include "sc_map_creator.hpp"
#include "sc_main.hpp"
#include "sc_play.hpp"
#include <vk.hpp>
#include <vkph_events.hpp>

static scene_type_t bound_scene;

static vk::eye_3d_info_t eye_info;
static vk::lighting_info_t lighting_info;

static vkph::listener_t scene_listener;

static void s_scene_event_listener(
    void *object,
    vkph::event_t *event) {
    switch (bound_scene) {
    case ST_GAME_PLAY: {
        sc_handle_play_event(object, event);
    } break;

    case ST_MAIN_MENU: {
        sc_handle_main_event(object, event);
    } break;

    case ST_MAP_CREATOR: {
        sc_handle_map_creator_event(object, event);
    } break;

    default: {
    } break;
    }
}

void sc_scenes_init(vkph::state_t *state) {
    scene_listener = vkph::set_listener_callback(s_scene_event_listener, state);

    sc_main_init(scene_listener);
    sc_play_init(scene_listener);
    sc_map_creator_init(scene_listener);
}

void sc_bind(scene_type_t scene_type, vkph::state_t *state) {
    bound_scene = scene_type;

    switch (scene_type) {
    case ST_MAIN_MENU: {
        sc_bind_main();
    } break;

    case ST_GAME_PLAY: {
        sc_bind_play(state);
    } break;

    case ST_MAP_CREATOR: {
        sc_bind_map_creator(state);
    } break;

    default: {
    } break;
    }
}

void sc_tick(
    VkCommandBuffer render,
    VkCommandBuffer transfer,
    VkCommandBuffer ui,
    VkCommandBuffer shadow,
    vkph::state_t *state) {
    switch (bound_scene) {
    case ST_MAIN_MENU: {
        sc_main_tick(render, transfer, ui, state);
    } break;

    case ST_GAME_PLAY: {
        sc_play_tick(render, transfer, ui, shadow, state);
    } break;

    case ST_MAP_CREATOR: {
        sc_map_creator_tick(render, transfer, ui, shadow, state);
    } break;

    default: {
    } break;
    }
}

vk::eye_3d_info_t *sc_get_eye_info() {
    return &eye_info;
}

vk::lighting_info_t *sc_get_lighting_info() {
    return &lighting_info;
}
