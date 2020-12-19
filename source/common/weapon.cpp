#include "weapon.hpp"
#include "constant.hpp"
#include "game.hpp"
#include "containers.hpp"

void weapon_t::init(
    uint32_t max_ammunition,
    uint32_t clip_size,
    firing_type_t f_type,
    bullet_type_t b_type,
    weapon_type_t w_type,
    float rps) {
    max_ammo = max_ammunition;
    max_clip_size = clip_size;
    firing_type = f_type;
    bullet_type = b_type;
    type = w_type;
    rounds_per_sec = rps;

    // Initialise just ONE clip
    clip_size = max_clip_size;
    ammo_left = max_ammo - max_clip_size;

    elapsed = 0.0f;
    recoil_time = 1.0f / rps;
    ready = 1;

    active_projs.init(max_ammo);
}

void weapon_t::weapon_reload() {
    if (ammo_left < max_clip_size) {
        clip_size = ammo_left;
        ammo_left = 0;
    }
    else {
        clip_size = max_clip_size;
        ammo_left -= max_clip_size;
    }
}

void tick_rock(rock_t *rock, float dt) {
    rock->position += rock->direction * dt;
    rock->direction -= rock->up * dt * GRAVITY_ACCELERATION;
}

bool check_projectile_players_collision(rock_t *rock, int32_t *dst_player) {
    // Check if there was collision with players or terrain
    ivector3_t chunk_coord = space_voxel_to_chunk(space_world_to_voxel(rock->position));
    chunk_t *c = g_game->access_chunk(chunk_coord);

    if (c) {
        for (uint32_t i = 0; i < c->players_in_chunk.data_count; ++i) {
            uint8_t player_local_id = c->players_in_chunk[i];

            player_t *p = g_game->get_player(player_local_id);

            if (p->client_id != rock->client_id) {
                if (collide_sphere_with_player(p, rock->position, 0.2f)) {
                    // Collision!
                    *dst_player = (int32_t)player_local_id;

                    return true;
                }
            }
        }
    }
    
    return false;
}

bool check_projectile_terrain_collision(rock_t *rock) {
    terrain_collision_t collision = {};
    collision.ws_size = vector3_t(0.2f);
    collision.ws_position = rock->position;
    collision.ws_velocity = rock->direction;
    collision.es_position = collision.ws_position / collision.ws_size;
    collision.es_velocity = collision.ws_velocity / collision.ws_size;

    check_ray_terrain_collision(&collision);
    if (collision.detected) {
        return true;
    }
    else {
        return false;
    }
}
