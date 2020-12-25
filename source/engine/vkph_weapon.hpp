#pragma once

#include <stdint.h>
#include <common/containers.hpp>

namespace vkph {

enum class firing_type_t { AUTOMATIC, SEMI_AUTOMATIC, INVALID };
enum class bullet_type_t { PROJECTILE, HITSCAN, INVALID };

// TODO: ADD MORE WEPANOS!
enum class weapon_type_t { TERRAFORMER, ROCKS, EXPLODING_ROCKS, RIFLE, INVALID };

/*
  Contains the index of a specific projectile in state_t::rocks / projectile_type.
 */
struct projectile_obj_reference_t {
    uint32_t initialised: 1;
    uint32_t idx: 31;
};

struct weapon_t {
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

    stack_container_t<projectile_obj_reference_t> active_projs;

    // Methods
    void init(
        uint32_t max_ammo,
        uint32_t clip_size,
        firing_type_t f_type,
        bullet_type_t b_type,
        weapon_type_t w_type,
        float rps);

    void weapon_reload();
};

}
