#include "common/event.hpp"
#include "srv_main.hpp"
#include <common/game.hpp>
#include <common/chunk.hpp>
#include <common/player.hpp>

static listener_t game_listener;

static void s_handle_event_spawn(event_t *event) {
    event_spawn_t *data = (event_spawn_t *)event->data;
    uint32_t id = data->client_id;

    LOG_INFOV("Client %i spawned\n", data->client_id);

    int32_t local_id = translate_client_to_local_id(id);
    player_t *p = get_player(local_id);
    p->ws_position = p->next_random_spawn_position;
    p->ws_view_direction = glm::normalize(-p->ws_position);
    // Calculate up vector
    vector3_t right = glm::cross(p->ws_view_direction, vector3_t(0.0f, 1.0f, 0.0f));
    p->ws_up_vector = glm::cross(right, p->ws_view_direction);
    p->flags.alive_state = PAS_ALIVE;
    // Meteorite when player spawns
    p->flags.interaction_mode = PIM_METEORITE;
    p->ws_velocity = vector3_t(0.0f);
}

static void s_handle_event_new_player(event_t *event) {
    event_new_player_t *data = (event_new_player_t *)event->data;

    player_t *player = add_player();
    fill_player_info(player, &data->info);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(
    event_t *event) {
    event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

    int32_t local_id = translate_client_to_local_id(data->client_id);
    player_t *p = get_player(local_id);
            
    if (p) {
        remove_player(p->local_id);
    }

    FL_FREE(event->data);
}

static void s_game_listener(void *object, event_t *event, event_submissions_t *events) {
    switch(event->type) {

    case ET_SPAWN: {
        s_handle_event_spawn(event);
    } break;
        
    case ET_NEW_PLAYER: {
        s_handle_event_new_player(event);
    } break;

    case ET_PLAYER_DISCONNECTED: {
        s_handle_event_player_disconnected(event);
    } break;

    default: {
    } break;

    }
    
}

void srv_game_init(event_submissions_t *events) {
    game_listener = set_listener_callback(&s_game_listener, NULL, events);

    subscribe_to_event(ET_NEW_PLAYER, game_listener, events);
    subscribe_to_event(ET_PLAYER_DISCONNECTED, game_listener, events);
    subscribe_to_event(ET_SPAWN, game_listener, events);

    player_memory_init();
    chunk_memory_init();

    generate_sphere(vector3_t(20.0f), 40.0f);
    generate_sphere(vector3_t(-40.0f, 40.0f, -40.0f), 20.0f);
    generate_sphere(vector3_t(-70.0f, 90.0f, 45.0f), 25.0f);
}

void srv_game_tick() {
    for (uint32_t i = 0; i < get_player_count(); ++i) {
        player_t *player = get_player(i);

        if (player) {
            if (player->flags.alive_state == PAS_ALIVE) {
                // Execute all received player actions
                for (uint32_t i = 0; i < player->player_action_count; ++i) {
                    player_action_t *action = &player->player_actions[i];

                    execute_action(player, action);
                }

                player->player_action_count = 0;
            }
        }
    }
}
