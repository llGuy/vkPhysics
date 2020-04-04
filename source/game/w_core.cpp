#include "net.hpp"
#include <imgui.h>
#include "world.hpp"
#include "engine.hpp"
#include "w_internal.hpp"
#include <renderer/input.hpp>
#include <renderer/renderer.hpp>

static mesh_t cube = {};
static shader_t cube_shader;
static mesh_render_data_t cube_data = {};

static mesh_t sphere = {};
static shader_t sphere_shader;
static mesh_render_data_t render_data = {};

// Temporary
static struct player_info_t {
    vector3_t position;
    vector3_t direction;
} player;

static world_t world;

static void s_world_event_listener(
    void *object,
    event_t *event) {
    switch(event->type) {

    case ET_ENTER_SERVER: {
        // Initialise chunks / players
    } break;

    case ET_NEW_PLAYER: {
        event_new_player_t *data = (event_new_player_t *)event->data;

        player_t *p = w_add_player(&world);
        
        if (data->client_data) {
            p->name = data->client_data->name;
            p->client_id = data->client_data->client_id;
            // Now the network module can use w_get_player_from_client_id to get access to player directly
            w_link_client_id_to_local_id(p->client_id, p->local_id, &world);
        }
        
        p->ws_position = data->ws_position;
        p->ws_view_direction = data->ws_view_direction;
        p->ws_up_vector = data->ws_up_vector;
        p->player_action_count = 0;
        p->default_speed = data->default_speed;
        memset(p->player_actions, 0, sizeof(p->player_actions));

        if (data->is_local) {
            w_set_local_player(p->local_id, &world);
        }
    } break;

    }
}

static listener_t world_listener;

void world_init(
    event_submissions_t *events) {
    world_listener = set_listener_callback(s_world_event_listener, NULL, events);
    subscribe_to_event(ET_ENTER_SERVER, world_listener, events);
    subscribe_to_event(ET_NEW_PLAYER, world_listener, events);

    memset(&world, 0, sizeof(world_t));

    w_players_data_init();
    w_player_world_init(&world);

    w_chunks_data_init();
    w_chunk_world_init(&world, 4);

    player.position = vector3_t(100.0f);
    player.direction = vector3_t(1.0f, 0.0f, 0.0f);
}

void destroy_world() {
    w_destroy_chunk_world(&world);
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
#if 1
    w_tick_players(&world);

    w_players_gpu_sync_and_render(
        render_command_buffer,
        transfer_command_buffer,
        &world);

    w_chunk_gpu_sync_and_render(
        render_command_buffer,
        transfer_command_buffer,
        &world);
#endif
    
    if (render_command_buffer != VK_NULL_HANDLE) {
        render_environment(render_command_buffer);
    }

#if 0
    cube_data.model = glm::scale(vector3_t(20.0f, 0.3f, 20.0f));
    cube_data.color = vector4_t(0.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(render_command_buffer, &cube, &cube_shader, &cube_data);

    cube_data.model = glm::translate(vector3_t(15.0f, 0.0f, 0.0f)) * glm::rotate(glm::radians(60.0f), vector3_t(1.0f, 1.0f, 0.0f)) * glm::scale(vector3_t(2.0f, 10.0f, 1.0f));
    cube_data.color = vector4_t(1.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(render_command_buffer, &cube, &cube_shader, &cube_data);

    for (uint32_t x = 0; x < 7; ++x) {
        for (uint32_t y = 0; y < 7; ++y) {
            vector2_t xz = vector2_t((float)x, (float)y);

            xz -= vector2_t(3.5f);
            xz *= 3.5f;

            vector3_t ws_position = vector3_t(xz.x, 1.25f, xz.y);

            render_data.model = glm::translate(ws_position);
            render_data.pbr_info.x = glm::clamp((float)x / 7.0f, 0.05f, 1.0f);
            render_data.pbr_info.y = (float)y / 7.0f;
                
            submit_mesh(render_command_buffer, &sphere, &sphere_shader, &render_data);
        }
    }
#endif
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
    info.ws_light_positions[0] = vector4_t(player.position, 1.0f);
    info.ws_light_directions[0] = vector4_t(player.direction, 0.0f);
    info.light_colors[0] = vector4_t(100.0f);
    info.lights_count = 0;

    return info;
}
