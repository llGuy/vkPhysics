#pragma once

#include <stdint.h>
#include <math.hpp>

namespace vkph {

struct predicted_projectile_hit_t {
    struct {
        uint16_t initialised: 1;
    } flags;

    uint16_t client_id;
    uint64_t tick_before;
    uint64_t tick_after;
    float progression;
};

// ROCK ///////////////////////////////////////////////////////////////////////
struct rock_snapshot_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    uint16_t client_id;
};

// All the different types of projectiles which will deal damage (e.g. rocks, exploding rocks, etc...)
struct rock_t {
    static constexpr uint32_t DIRECT_DAMAGE = 75;

    struct {
        uint32_t active: 1;
        uint32_t spawned_locally: 1;

        /* 
          Index of the reference to this projectile in the weapon structure's list of active projectiles
        */
        uint32_t ref_idx_obj: 28;
        uint32_t ref_idx_weapon: 2;
    } flags;

    vector3_t position;
    vector3_t direction;
    vector3_t up;
    // The ID of the client who spawned this rock
    uint16_t client_id;

    rock_t() = default;

    rock_t(
        const vector3_t &p,
        const vector3_t &d,
        const vector3_t &u,
        uint16_t cid,
        uint32_t ref_obj,
        uint32_t ref_weapon)
        : position(p), direction(d), up(u), client_id(cid) {
        flags.active = 1;
        flags.ref_idx_obj = ref_obj;
        flags.ref_idx_weapon = ref_weapon;
    }

    void tick(float dt);
};

}
