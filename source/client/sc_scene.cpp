#include "sc_scene.hpp"
#include "client/sc_map_creator.hpp"
#include "sc_main.hpp"
#include "sc_play.hpp"
#include <vk.hpp>
#include <common/event.hpp>

static scene_type_t bound_scene;

static vk::eye_3d_info_t eye_info;
static vk::lighting_info_t lighting_info;

static listener_t scene_listener;

static void s_scene_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    switch (bound_scene) {
    case ST_GAME_PLAY: {
        sc_handle_play_event(object, event, events);
    } break;

    case ST_MAIN_MENU: {
        sc_handle_main_event(object, event, events);
    } break;

    case ST_MAP_CREATOR: {
        sc_handle_map_creator_event(object, event, events);
    } break;

    default: {
    } break;
    }
}

void sc_scenes_init(event_submissions_t *events) {
    scene_listener = set_listener_callback(s_scene_event_listener, NULL, events);

    sc_main_init(scene_listener, events);
    sc_play_init(scene_listener, events);
    sc_map_creator_init(scene_listener, events);
}

void sc_bind(scene_type_t scene_type) {
    bound_scene = scene_type;

    switch (scene_type) {
    case ST_MAIN_MENU: {
        sc_bind_main();
    } break;

    case ST_GAME_PLAY: {
        sc_bind_play();
    } break;

    case ST_MAP_CREATOR: {
        sc_bind_map_creator();
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
    event_submissions_t *events) {
    switch (bound_scene) {
    case ST_MAIN_MENU: {
        sc_main_tick(render, transfer, ui, events);
    } break;

    case ST_GAME_PLAY: {
        sc_play_tick(render, transfer, ui, shadow, events);
    } break;

    case ST_MAP_CREATOR: {
        sc_map_creator_tick(render, transfer, ui, shadow, events);
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
