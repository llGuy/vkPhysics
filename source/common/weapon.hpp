#pragma once

#include "tools.hpp"
#include "containers.hpp"

#include <utility>

enum class firing_type_t { AUTOMATIC, SEMI_AUTOMATIC, INVALID };
enum class bullet_type_t { PROJECTILE, HITSCAN, INVALID };

// TODO: ADD MORE WEPANOS!
enum class weapon_type_t { TERRAFORMER, ROCKS, EXPLODING_ROCKS, RIFLE, INVALID };

struct projectile_obj_reference_t {
    uint32_t initialised: 1;
    uint32_t idx: 31;
};

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

    stack_container_t<projectile_obj_reference_t> active_projs;
};

struct rock_snapshot_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    uint16_t client_id;
};

// All the different types of projectiles which will deal damage (e.g. rocks, exploding rocks, etc...)
struct rock_t {
    struct {
        uint32_t active: 1;
        uint32_t spawned_locally: 1;

        // Index of the reference to this projectile
        // In the weapon structure's list of active projectiles
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

    static constexpr uint32_t DIRECT_DAMAGE = 75;
};

// TODO
struct exploding_rock_t {
    vector3_t position;
    //....
};

// Initialise all the memory to contain the rocks / bullets which will deal damage
void weapons_and_bullets_memory_init();

void tick_rock(rock_t *rock, float dt);

template <typename T, uint32_t MAX_NEW_PROJECTILES_PER_FRAME>
struct projectile_tracker_t {
    stack_container_t<T> list;

    // Projectiles that were spawned in the past frame
    uint32_t recent_count;
    uint32_t recent[MAX_NEW_PROJECTILES_PER_FRAME];

    void init() {
         // Projectiles
        list.init(MAX_NEW_PROJECTILES_PER_FRAME);

        recent_count = 0;
        for (uint32_t i = 0; i < 50; ++i) {
            recent[i] = 0;
        }
    }

    template <typename ...Constr /* Constructor parameters */>
    uint32_t spawn(Constr &&...params) {
        // Add the projectile to the list
        uint32_t idx = list.add();
        T *p = &list[idx];
        *p = T(std::forward<Constr>(params)...);

        recent[recent_count++] = idx;

        return idx;
    }

    void clear_recent() {
        recent_count = 0;
    }
};

bool check_projectile_players_collision(rock_t *rock, int32_t *dst_player);
bool check_projectile_terrain_collision(rock_t *rock);

struct predicted_projectile_hit_t {
    struct {
        uint16_t initialised: 1;
    } flags;

    uint16_t client_id;
    uint64_t tick_before;
    uint64_t tick_after;
    float progression;
};
