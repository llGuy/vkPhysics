#pragma once

#include "tools.hpp"

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
