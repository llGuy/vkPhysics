#include "vkph_constant.hpp"
#include "vkph_projectile.hpp"

namespace vkph {

void rock_t::tick(float dt) {
    position += direction * dt;
    direction -= up * dt * GRAVITY_ACCELERATION;
}

}
