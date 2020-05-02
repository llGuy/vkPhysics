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

// Linear interpolation (constant speed)
template <typename T> struct smooth_linear_interpolation_t {
    bool in_animation;

    T current;
    T prev;
    T next;
    float current_time;
    float max_time;

    void animate(
        float dt) {
        if (in_animation) {
            current_time += dt;
            float progression = current_time / max_time;
        
            if (progression >= 1.0f) {
                in_animation = 0;
                current = next;
            }
            else {
                current = prev + progression * (next - prev);
            }
        }
    }
};


typedef bool (*smooth_exponential_interpolation_compare_float_t)(float current, float destination);
inline bool smooth_exponential_interpolation_compare_float(float current, float destination) {
    return(abs(destination - current) < 0.001f);
}


// Starts fast, then slows down - this name is utter BS - I don't know the mathy name for it, it just looks like an exponential function
template <typename T, typename Compare> struct smooth_exponential_interpolation_t {
    bool in_animation;
    
    T destination;
    T current;
    float speed = 1.0f;

    Compare compare;

    void animate(
        float dt) {
        if (in_animation) {
            current += (destination - current) * dt * speed;
            if (compare(current, destination)) {
                in_animation = 0;
                current = destination;
            }
        }
    }
};

