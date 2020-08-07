#include "gm_mode.hpp"
#include "gm_main.hpp"
#include "gm_play.hpp"
#include <renderer/renderer.hpp>
#include <common/event.hpp>

static game_mode_type_t bound_game_mode;

static eye_3d_info_t eye_info;
static lighting_info_t lighting_info;

static listener_t game_mode_listener;

static void s_game_mode_event_listener(
    void *object,
    event_t *event,
    event_submissions_t *events) {
    switch (bound_game_mode) {
    case GMT_GAME_PLAY: {
        gm_handle_play_event(object, event, events);
    } break;

    case GMT_MAIN_MENU: {
        gm_handle_main_event(object, event, events);
    } break;

    default: {
    } break;
    }
}

void gm_modes_init(event_submissions_t *events) {
    game_mode_listener = set_listener_callback(s_game_mode_event_listener, NULL, events);

    gm_main_init(game_mode_listener, events);
    gm_play_init(game_mode_listener, events);
}

void gm_bind(game_mode_type_t mode) {
    bound_game_mode = mode;

    switch (mode) {
    case GMT_MAIN_MENU: {
        gm_bind_main();
    } break;

    case GMT_GAME_PLAY: {
        gm_bind_play();
    } break;

    default: {
    } break;
    }
}

void gm_tick(VkCommandBuffer render, VkCommandBuffer transfer, VkCommandBuffer ui, event_submissions_t *events) {
    switch (bound_game_mode) {
    case GMT_MAIN_MENU: {
        gm_main_tick(render, transfer, ui, events);
    } break;

    case GMT_GAME_PLAY: {
        gm_play_tick(render, transfer, ui, events);
    } break;

    default: {
    } break;
    }
}

eye_3d_info_t *gm_get_eye_info() {
    return &eye_info;
}

lighting_info_t *gm_get_lighting_info() {
    return &lighting_info;
}
