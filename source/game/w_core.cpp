# include "ai.hpp"
#include <cstddef>
#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "w_internal.hpp"
#include <common/log.hpp>
#include "common/event.hpp"
#include <common/files.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>
#include <common/serialiser.hpp>

static context_t context;

void world_init(
    event_submissions_t *events) {
    context.world_listener = set_listener_callback(w_world_event_listener, NULL, events);
    w_subscribe_to_events(&context, context.world_listener, events);

    memset(&context, 0, sizeof(context_t));

    w_create_shaders_and_meshes();

    w_player_world_init();

    w_allocate_temp_vertices_for_chunk_mesh_creation();
    w_chunk_world_init(4);

    context.in_meta_menu = 1;
    w_startup_init();
}

void destroy_world() {
    w_clear_chunk_world();
    w_free_temp_vertices_for_chunk_mesh_creation();
}

void handle_world_input(
    highlevel_focus_t focus) {
    if (context.in_meta_menu) {
        w_handle_spectator_mouse_movement();
    }
    else if (focus == HF_WORLD) {
        game_input_t *game_input = get_game_input();
        w_handle_input(game_input, surface_delta_time());
    }
}

static void s_check_training_ai() {
    // switch (world.training_type) {
    // case ATS_ROLLING: {
        
    // } break;
    // case ATS_WALKING: {
    //     vector3_t pos = world.players[0]->ws_position;
    // } break;
    // }
}

void tick_world(
    event_submissions_t *events) {
    (void)events;
    w_tick_chunks(logic_delta_time());
    w_tick_players(events);

#if 1
    // AI training stuff
#endif
}

void write_startup_screen() {
    // w_write_startup_screen();
}

void kill_local_player(
    event_submissions_t *events) {
    LOG_INFO("Local player just died\n");

    submit_event(ET_LAUNCH_GAME_MENU_SCREEN, NULL, events);
    submit_event(ET_ENTER_SERVER_META_MENU, NULL, events);

    w_get_local_player()->flags.alive_state = PAS_DEAD;
}

