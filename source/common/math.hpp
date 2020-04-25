#pragma once

#include "tools.hpp"
#include <glm/gtx/hash.hpp>

inline vector3_t interpolate(
    const vector3_t &a,
    const vector3_t &b,
    float x) {
    return(a + x * (b - a));
}

inline float interpolate(
    float a,
    float b,
    float x) {
    return(a + x * (b - a));
}

inline float lerp(
    float a,
    float b,
    float x) {
    return((x - a) / (b - a));
}

inline float distance_squared(
    const vector3_t &a) {
    return glm::dot(a, a);
}

inline float squared(
    float a) {
    return a * a;
}
