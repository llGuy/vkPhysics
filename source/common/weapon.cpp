#include "weapon.hpp"
#include "common/containers.hpp"

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
    ready = 1;
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
