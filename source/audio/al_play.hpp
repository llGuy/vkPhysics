#pragma once

#include <math.hpp>

namespace al {

/* 
   For now, just have one listener at 0,0,0
   All 3D sounds will be played "relative" to the
   player position
   
   All UI sounds will just be played at position 0,0,0
*/
void init_listener();
void set_listener_velocity(const vector3_t &velocity);

}
