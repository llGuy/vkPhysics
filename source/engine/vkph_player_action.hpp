#pragma once

#include <stdint.h>

namespace vkph {

/*
  There can be multiple of these per game tick (they can be sent over the network).
 */
struct player_action_t {

    union {

        struct {
            uint16_t move_forward: 1;
            uint16_t move_left: 1;
            uint16_t move_back: 1;
            uint16_t move_right: 1;
            uint16_t jump: 1;
            uint16_t crouch: 1;
            uint16_t trigger_left: 1;
            uint16_t trigger_right: 1;
            // Switch between player mode and ball mode
            uint16_t switch_shapes: 1;
            uint16_t flashlight: 1;
            uint16_t dead: 1;
            uint16_t switch_weapons: 1;
            // All 1s is an invalid weapon. The player is just cycling through weapons
            uint16_t next_weapon: 3;
        };

        uint16_t bytes;
    };
    
    float dmouse_x;
    float dmouse_y;
    float dt;

    // This is for terraforming
    float accumulated_dt;

    // For debugging
#if 1
    uint64_t tick;
#endif

};

}
