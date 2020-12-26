#include "srv_main.hpp"
#include <vkph_chunk.hpp>
#include <vkph_state.hpp>
#include <vkph_events.hpp>
#include <vkph_physics.hpp>
#include <vkph_event_data.hpp>

#include <common/net.hpp>

static vkph::listener_t game_listener;

void spawn_player(uint32_t client_id, vkph::state_t *state) {
    LOG_INFOV("Client %i spawned\n", client_id);

    int32_t local_id = state->get_local_id(client_id);
    vkph::player_t *p = state->get_player(local_id);
    p->health = 200;
    p->ws_position = p->next_random_spawn_position;
    p->ws_view_direction = glm::normalize(-p->ws_position);
    // Calculate up vector
    vector3_t right = glm::cross(p->ws_view_direction, vector3_t(0.0f, 1.0f, 0.0f));
    p->ws_up_vector = glm::cross(right, p->ws_view_direction);
    p->flags.is_alive = true;
    // Meteorite when player spawns
    p->flags.interaction_mode = vkph::PIM_METEORITE;
    p->ws_velocity = vector3_t(0.0f);

    // Generate a new random position next time player needs to spawn
    float x_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float y_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    float z_rand = (float)(rand() % 100 + 100) * (rand() % 2 == 0 ? -1 : 1);
    p->next_random_spawn_position = vector3_t(x_rand, y_rand, z_rand);
}

static void s_handle_event_new_player(vkph::event_t *event, vkph::state_t *state) {
    vkph::event_new_player_t *data = (vkph::event_new_player_t *)event->data;

    vkph::player_t *player = state->add_player();
    player->init(&data->info, state->client_to_local_id_map);

    FL_FREE(event->data);
}

static void s_handle_event_player_disconnected(vkph::event_t *event, vkph::state_t *state) {
    auto *data = (vkph::event_player_disconnected_t *)event->data;

    int32_t local_id = state->get_local_id(data->client_id);
    vkph::player_t *p = state->get_player(local_id);
            
    if (p) {
        // Remove player from the team as well
        state->remove_player_from_team(p);

        state->remove_player(p->local_id);
    }

    FL_FREE(event->data);
}

static void s_game_listener(void *object, vkph::event_t *event) {
    vkph::state_t *state = (vkph::state_t *)object;

    switch(event->type) {

    case vkph::ET_NEW_PLAYER: {
        s_handle_event_new_player(event, state);
    } break;

    case vkph::ET_PLAYER_DISCONNECTED: {
        s_handle_event_player_disconnected(event, state);
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

void srv_game_init(vkph::state_t *state) {
    game_listener = set_listener_callback(&s_game_listener, state);

    vkph::subscribe_to_event(vkph::ET_NEW_PLAYER, game_listener);
    vkph::subscribe_to_event(vkph::ET_PLAYER_DISCONNECTED, game_listener);
    vkph::subscribe_to_event(vkph::ET_SPAWN, game_listener);

    state->prepare();

    state->configure_game_mode(vkph::game_mode_t::DEATHMATCH);
    state->configure_map("ice.map");
    state->configure_team_count(2);
    state->configure_team(0, vkph::team_color_t::BLUE, 10);
    state->configure_team(1, vkph::team_color_t::RED, 10);
    state->start_session();
}

// Compensate for lag
static bool s_check_projectile_player_collision_lag(
    vkph::rock_t *rock,
    int32_t *dst_player,
    vkph::state_t *state) {
    ivector3_t chunk_coord = vkph::space_voxel_to_chunk(vkph::space_world_to_voxel(rock->position));
    vkph::chunk_t *c = state->access_chunk(chunk_coord);

    bool collided = 0;

    if (c) {
        for (uint32_t i = 0; i < c->players_in_chunk.data_count; ++i) {
            uint8_t player_local_id = c->players_in_chunk[i];

            auto *target = state->get_player(player_local_id);

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
                if (target->flags.interaction_mode == vkph::PIM_STANDING ||
                    target->flags.interaction_mode == vkph::PIM_FLOATING) {
                    collided = vkph::collide_sphere_with_standing_player(
                        approx_target_position,
                        target->ws_up_vector,
                        rock->position,
                        0.2f);
                }
                else {
                    collided = vkph::collide_sphere_with_rolling_player(
                        approx_target_position,
                        rock->position,
                        0.2f);
                }

                if (collided) {
                    LOG_INFOV("%s just got hit by projectile\n", target->name);

                    // Register hit and decrease client's health
                    if (target->health < vkph::rock_t::DIRECT_DAMAGE) {
                        LOG_INFOV("%s just got killed\n", target->name);

                        // Player needs to die
                        target->flags.is_alive = false;
                        target->frame_displacement = 0.0f;
                    }

                    target->health -= vkph::rock_t::DIRECT_DAMAGE;

                    break;
                }
            }
        }
    }
    
    return collided;
}

void srv_game_tick(vkph::state_t *state) {
    for (uint32_t i = 0; i < state->players.data_count; ++i) {
        auto *player = state->get_player(i);

        if (player) {
            if (player->flags.is_alive) {
                // Execute all received player actions
                for (uint32_t i = 0; i < player->player_action_count; ++i) {
                    vkph::player_action_t *action = &player->player_actions[i];

                    player->execute_action(action, state);

                    if (!player->flags.is_alive) {
                        break;
                    }
                }

                player->player_action_count = 0;
            }
        }
    }

    // Still need to update all the things that update despite entities (projectiles)
    for (uint32_t i = 0; i < state->rocks.list.data_count; ++i) {
        vkph::rock_t *rock = &state->rocks.list[i];

        if (rock->flags.active) {
            int32_t target = -1;

            bool collided_with_terrain = vkph::check_projectile_terrain_collision(rock, state);
            bool collided_with_player = s_check_projectile_player_collision_lag(rock, &target, state);

            if (collided_with_player) {
                // Do something
                uint16_t client_id = rock->client_id;
                uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                uint32_t ref_idx = rock->flags.ref_idx_obj;

                auto *p = state->get_player(state->get_local_id(client_id));
                p->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                p->weapons[weapon_idx].active_projs.remove(ref_idx);

                rock->flags.active = 0;
                state->rocks.list.remove(i);
            }
            else if (collided_with_terrain) {
                uint16_t client_id = rock->client_id;
                uint32_t weapon_idx = rock->flags.ref_idx_weapon;
                uint32_t ref_idx = rock->flags.ref_idx_obj;

                auto *p = state->get_player(state->get_local_id(client_id));
                p->weapons[weapon_idx].active_projs[ref_idx].initialised = 0;
                p->weapons[weapon_idx].active_projs.remove(ref_idx);

                rock->flags.active = 0;
                state->rocks.list.remove(i);
            }

            rock->tick(state->dt);
        }
    }
}
