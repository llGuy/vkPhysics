#include "game.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include "constant.hpp"
#include <glm/gtx/projection.hpp>
#include <glm/gtx/string_cast.hpp>

bool collide_sphere_with_standing_player(
    const vector3_t &target,
    const vector3_t &target_up,
    const vector3_t &center,
    float radius) {
    float player_height = PLAYER_SCALE * 2.0f;

    // Check collision with 2 spheres
    float sphere_scale = player_height * 0.5f;
    vector3_t body_low = target + (target_up * player_height * 0.22f);
    vector3_t body_high = target + (target_up * player_height * 0.75f);

    vector3_t body_low_diff = body_low - center;
    vector3_t body_high_diff = body_high - center;

    float dist2_low = glm::dot(body_low_diff, body_low_diff);
    float dist2_high = glm::dot(body_high_diff, body_high_diff);

    float dist_min = radius + sphere_scale;
    float dist_min2 = dist_min * dist_min;

    if (dist2_low < dist_min2 || dist2_high < dist_min2) {
        return true;
    }
    else {
        return false;
    }
}

bool collide_sphere_with_rolling_player(const vector3_t &target, const vector3_t &center, float radius) {
    float dist_min = radius + PLAYER_SCALE;
    float dist_min2 = dist_min * dist_min;

    vector3_t diff = target - center;
    float dist_to_player2 = glm::dot(diff, diff);

    if (dist_to_player2 < dist_min2) {
        return true;
    }
    else {
        return false;
    }
}

bool collide_sphere_with_player(
    const player_t *p,
    const vector3_t &center,
    float radius) {
    if (p->flags.interaction_mode == PIM_STANDING ||
        p->flags.interaction_mode == PIM_FLOATING) {
        return collide_sphere_with_standing_player(
            p->ws_position,
            p->ws_up_vector,
            center,
            radius);
    }
    else {
        return collide_sphere_with_rolling_player(
            p->ws_position,
            center,
            radius);
    }

    return false;
}
