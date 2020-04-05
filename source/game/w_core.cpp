#include "net.hpp"
#include "world.hpp"
#include "engine.hpp"
#include "w_internal.hpp"
#include <common/log.hpp>
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

static world_t world;

static void s_add_player_from_info(
    player_init_info_t *init_info) {
    player_t *p = w_add_player(&world);
        
    if (init_info->client_data) {
        p->name = init_info->client_data->name;
        p->client_id = init_info->client_data->client_id;
        // Now the network module can use w_get_player_from_client_id to get access to player directly
        w_link_client_id_to_local_id(p->client_id, p->local_id, &world);
    }
        
    p->ws_position = init_info->ws_position;
    p->ws_view_direction = init_info->ws_view_direction;
    p->ws_up_vector = init_info->ws_up_vector;
    p->player_action_count = 0;
    p->default_speed = init_info->default_speed;
    memset(p->player_actions, 0, sizeof(p->player_actions));

    if (init_info->is_local) {
        // If this is the local player (controlled by mouse and keyboard, need to cache all player actions to send to the server)
        w_set_local_player(p->local_id, &world);
        p->cached_player_action_count = 0;
        p->cached_player_actions = FL_MALLOC(player_actions_t, MAX_PLAYER_ACTIONS * 2);
    }

    LOG_INFOV("Added player %i: %s\n", p->local_id, p->name);
}

static void s_world_event_listener(
    void *,
    event_t *event) {
    switch(event->type) {

    case ET_ENTER_SERVER: {
        LOG_INFO("Entering server world\n");

        // Reinitialise chunks / players
        w_clear_players(&world);
        w_clear_chunk_world(&world);

        event_enter_server_t *data = (event_enter_server_t *)event->data;

        for (uint32_t i = 0; i < data->info_count; ++i) {
            s_add_player_from_info(&data->infos[i]);
        }

        FL_FREE(data->infos);
        FL_FREE(event->data);
    } break;

    case ET_LEAVE_SERVER: {
        w_clear_players(&world);
        w_clear_chunk_world(&world);
    } break;

    case ET_NEW_PLAYER: {
        event_new_player_t *data = (event_new_player_t *)event->data;

        s_add_player_from_info(&data->info);

        FL_FREE(event->data);
    } break;

    case ET_PLAYER_DISCONNECTED: {
        event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

        player_t *p = w_get_player_from_client_id(data->client_id, &world);

        w_destroy_player(p->local_id, &world);

        FL_FREE(event->data);
    } break;

    default: {
    } break;

    }
}

static listener_t world_listener;

void world_init(
    event_submissions_t *events) {
    world_listener = set_listener_callback(s_world_event_listener, NULL, events);
    subscribe_to_event(ET_ENTER_SERVER, world_listener, events);
    subscribe_to_event(ET_NEW_PLAYER, world_listener, events);
    subscribe_to_event(ET_LEAVE_SERVER, world_listener, events);
    subscribe_to_event(ET_PLAYER_DISCONNECTED, world_listener, events);

    memset(&world, 0, sizeof(world_t));

    w_players_data_init();
    w_player_world_init(&world);

    w_chunks_data_init();
    w_chunk_world_init(&world, 4);
}

void destroy_world() {
    w_clear_chunk_world(&world);
    w_destroy_chunk_data();
}

void handle_world_input() {
    game_input_t *game_input = get_game_input();
    
    w_handle_input(game_input, surface_delta_time(), &world);
}

void tick_world(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer,
    event_submissions_t *events) {
    (void)events;
    w_tick_players(&world);

    w_players_gpu_sync_and_render(
        render_command_buffer,
        transfer_command_buffer,
        &world);

    w_chunk_gpu_sync_and_render(
        render_command_buffer,
        transfer_command_buffer,
        &world);
    
    if (render_command_buffer != VK_NULL_HANDLE) {
        render_environment(render_command_buffer);
    }
}

eye_3d_info_t create_eye_info() {
    eye_3d_info_t info = {};

    player_t *player = w_get_local_player(&world);

    if (!player) {
        player = w_get_spectator(&world);
    }
    
    info.position = player->ws_position;
    info.direction = player->ws_view_direction;
    info.up = player->ws_up_vector;

    info.fov = 60.0f;
    info.near = 0.1f;
    info.far = 10000.0f;
    info.dt = surface_delta_time();

    return info;
}

lighting_info_t create_lighting_info() {
    lighting_info_t info = {};
    
    info.ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    /*info.ws_light_positions[0] = vector4_t(player.position, 1.0f);
    info.ws_light_directions[0] = vector4_t(player.direction, 0.0f);
    info.light_colors[0] = vector4_t(100.0f);*/
    info.lights_count = 0;

    return info;
}

struct player_t *get_player(
    uint16_t client_id) {
    return w_get_player_from_client_id(client_id, &world);
}
