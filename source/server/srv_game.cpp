#include "common/event.hpp"
#include "common/map.hpp"
#include "common/weapon.hpp"
#include "srv_main.hpp"
#include <common/game.hpp>
#include <common/chunk.hpp>
#include <common/player.hpp>

#include <common/net.hpp>

static listener_t game_listener;

void spawn_player(uint32_t client_id) {
    LOG_INFOV("Client %i spawned\n", client_id);

    int32_t local_id = g_game->get_local_id(client_id);
    player_t *p = g_game->get_player(local_id);
    p->health = 200;
    p->ws_position = p->next_random_spawn_position;
    p->ws_view_direction = glm::normalize(-p->ws_position);
    // Calculate up vector
    vector3_t right = glm::cross(p->ws_view_direction, vector3_t(0.0f, 1.0f, 0.0f));
    p->ws_up_vector = glm::cross(right, p->ws_view_direction);
    p->flags.alive_state = PAS_ALIVE;
    // Meteorite when player spawns
    p->flags.interaction_mode = PIM_METEORITE;
    p->ws_velocity = vector3_t(0.0f);

    // Generate a new random position next time player needs to spawn
    float x_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float y_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float z_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    p->next_random_spawn_position = vector3_t(x_rand, y_rand, z_rand);
}

static void s_handle_event_new_player(event_t *event) {
    event_new_player_t *data = (event_new_player_t *)event->data;

    player_t *player = g_game->add_player();
    fill_player_info(player, &data->info);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(
    event_t *event) {
    event_player_disconnected_t *data = (event_player_disconnected_t *)event->data;

    int32_t local_id = g_game->get_local_id(data->client_id);
    player_t *p = g_game->get_player(local_id);
            
    if (p) {
        // Remove player from the team as well
        g_game->remove_player_from_team(p);

        g_game->remove_player(p->local_id);
    }

    FL_FREE(event->data);
}

static void s_game_listener(void *object, event_t *event, event_submissions_t *events) {
    switch(event->type) {

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

static float s_bumps(float x, float y, float z) {
    float right = sin(0.1f * x) * cos(0.1f * y)  * 10.0f;

    float dist = fabs(right - z);

    if (dist < 4.0f) {
        return 1.0f - (dist / 4.0f);
    }
    else {
        return 0.0f;
    }
}

static float s_tube(float x, float y, float z) {
    x *= 0.1f;
    y *= 0.1f;
    z *= 0.1f;

    float right = 1.0f / (15.0f * (x * x + y * y));

    float dist = fabs(right - z);

    if (dist < 4.0f) {
        return 1.0f - (dist / 4.0f);
    }
    else {
        return 0.0f;
    }
}

void srv_game_init(event_submissions_t *events) {
    game_listener = set_listener_callback(&s_game_listener, NULL, events);

    subscribe_to_event(ET_NEW_PLAYER, game_listener, events);
    subscribe_to_event(ET_PLAYER_DISCONNECTED, game_listener, events);
    subscribe_to_event(ET_SPAWN, game_listener, events);

    g_game->init_memory();

    // Make this a parameter to the vkPhysics_server program
    // generate_sphere(vector3_t(0.0f), 30, 180, GT_ADDITIVE, 0b11111111);
    // load_map("nucleus.map");

    g_game->configure_game_mode(game_mode_t::DEATHMATCH);
    g_game->configure_map("ice.map");
    g_game->configure_team_count(2);
    g_game->configure_team(0, team_color_t::PURPLE, 10);
    g_game->configure_team(1, team_color_t::YELLOW, 10);

    g_game->start_session();
}

// Compensate for lag
static bool s_check_projectile_player_collision_lag(
    rock_t *rock,
    int32_t *dst_player) {
    ivector3_t chunk_coord = space_voxel_to_chunk(space_world_to_voxel(rock->position));
    chunk_t *c = g_game->access_chunk(chunk_coord);

    bool collided = 0;

    if (c) {
        for (uint32_t i = 0; i < c->players_in_chunk.data_count; ++i) {
            uint8_t player_local_id = c->players_in_chunk[i];

            player_t *target = g_game->get_player(player_local_id);

            // Get lag compensated position of the player
            auto *shooter_client = &g_net_data.clients[rock->client_id];
            float shooter_half_roundtrip = shooter_client->ping / 2.0f;

            auto *target_client = &g_net_data.clients[target->client_id];
            float target_half_roundtrip = target_client->ping / 2.0f;

            // Get the two snapshots that encompass the latency of the shooting_player
            float snapshot_from_head = (shooter_half_roundtrip + target_half_roundtrip) / NET_SERVER_SNAPSHOT_OUTPUT_INTERVAL;
            float snapshot_from_head_trunc = floor(snapshot_from_head);
            float progression = snapshot_from_head - snapshot_from_head_trunc;

            uint32_t s1_idx = target_client->previous_locations.decrement_index(
                target_client->previous_locations.head,
                (uint32_t)snapshot_from_head_trunc);

            uint32_t s0_idx = target_client->previous_locations.decrement_index(
                s1_idx);

            auto *s1 = &target_client->previous_locations.buffer[s1_idx];
            auto *s0 = &target_client->previous_locations.buffer[s0_idx];

            vector3_t approx_target_position = s0->ws_position + (s1->ws_position - s0->ws_position) * progression;

            if (target->client_id != rock->client_id) {
                if (target->flags.interaction_mode == PIM_STANDING ||
                    target->flags.interaction_mode == PIM_FLOATING) {
                    collided = collide_sphere_with_standing_player(
                        approx_target_position,
                        target->ws_up_vector,
                        rock->position,
                        0.2f);
                }
                else {
                    collided = collide_sphere_with_rolling_player(
                        approx_target_position,
                        rock->position,
                        0.2f);
                }

                if (collided) {
                    LOG_INFOV("%s just got hit by projectile\n", target->name);

                    // Register hit and decrease client's health
                    if (target->health < rock_t::DIRECT_DAMAGE) {
                        LOG_INFOV("%s just got killed\n", target->name);

                        // Player needs to die
                        target->flags.alive_state = PAS_DEAD;
                        target->frame_displacement = 0.0f;
                    }

                    target->health -= rock_t::DIRECT_DAMAGE;

                    break;
                }
            }
        }
    }
    
    return collided;
}

void srv_game_tick() {
    for (uint32_t i = 0; i < g_game->players.data_count; ++i) {
        player_t *player = g_game->get_player(i);

        if (player) {
            if (player->flags.alive_state == PAS_ALIVE) {
                // Execute all received player actions
                for (uint32_t i = 0; i < player->player_action_count; ++i) {
                    player_action_t *action = &player->player_actions[i];

                    execute_action(player, action);

                    if (player->flags.alive_state == PAS_DEAD) {
                        break;
                    }
                }

                player->player_action_count = 0;
            }
        }
    }

    // Still need to update all the things that update despite entities (projectiles)
    for (uint32_t i = 0; i < g_game->rocks.list.data_count; ++i) {
        rock_t *rock = &g_game->rocks.list[i];

        if (rock->flags.active) {
            int32_t target = -1;

            bool collided_with_terrain = check_projectile_terrain_collision(rock);
            bool collided_with_player = s_check_projectile_player_collision_lag(rock, &target);

            if (collided_with_player) {
                // Do something
                uint16_t client_id = rock->client_id;
                uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                uint32_t ref_idx = rock->flags.ref_idx_obj;

                auto *p = g_game->get_player(g_game->get_local_id(client_id));
                p->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                p->weapons[weapon_idx].active_projs.remove(ref_idx);

                rock->flags.active = 0;
                g_game->rocks.list.remove(i);
            }
            else if (collided_with_terrain) {
                uint16_t client_id = rock->client_id;
                uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                uint32_t ref_idx = rock->flags.ref_idx_obj;

                auto *p = g_game->get_player(g_game->get_local_id(client_id));
                p->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                p->weapons[weapon_idx].active_projs.remove(ref_idx);

                rock->flags.active = 0;
                g_game->rocks.list.remove(i);
            }

            tick_rock(rock, g_game->dt);
        }
    }
}
