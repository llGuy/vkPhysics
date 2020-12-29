#pragma once

#include <utility>
#include <stdint.h>
#include <containers.hpp>

namespace vkph {

template <typename Projectile, uint32_t MAX_NEW_PROJECTILES_PER_FRAME>
struct projectile_tracker_t {
    stack_container_t<Projectile> list;

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
        Projectile *p = &list[idx];
        *p = Projectile(std::forward<Constr>(params)...);

        recent[recent_count++] = idx;

        return idx;
    }

    void clear_recent() {
        recent_count = 0;
    }
};


}
