#pragma once

#include "tools.hpp"

enum class firing_type_t { AUTOMATIC, SEMI_AUTOMATIC, INVALID };
enum class bullet_type_t { PROJECTILE, HITSCAN, INVALID };
// TODO: ADD MORE WEPANOS!
enum class weapon_type_t { TERRAFORMER, ROCKS, EXPLODING_ROCKS, RIFLE, INVALID };

// Causes projectiles to spawn
struct weapon_t {
    // Methods
    void init(
        uint32_t max_ammo,
        uint32_t clip_size,
        firing_type_t f_type,
        bullet_type_t b_type,
        weapon_type_t w_type,
        float rps);

    void weapon_reload();



    // Current values
    uint32_t clip_size;
    uint32_t ammo_left;

    // Theoretical maximum values
    uint32_t max_ammo;
    uint32_t max_clip_size;

    firing_type_t firing_type;
    bullet_type_t bullet_type;
    weapon_type_t type;

    float rounds_per_sec;
    float recoil_time;
    float elapsed;
    bool ready;
};

// All the different types of projectiles which will deal damage (e.g. rocks, exploding rocks, etc...)
struct rock_t {
    struct {
        uint32_t active: 1;
        uint32_t spawned_locally: 1;
    } flags;

    vector3_t position;
    vector3_t direction;
    vector3_t up;
};

// TODO
struct exploding_rock_t {
    vector3_t position;
    //....
};

// Initialise all the memory to contain the rocks / bullets which will deal damage
void weapons_and_bullets_memory_init();

void spawn_rock(
    const vector3_t &position,
    const vector3_t &start_direction);

void tick_rock(rock_t *rock, float dt);
