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

inline float distance_squared(
    const vector3_t &a) {
    return glm::dot(a, a);
}

inline float squared(
    float a) {
    return a * a;
}

// Linear interpolation (constant speed)
struct smooth_linear_interpolation_t {
    bool in_animation;

    float current;
    float prev;
    float next;
    float current_time;
    float max_time;

    void set(
        bool iin_animation,
        float iprev,
        float inext,
        float imax_time) {
        in_animation = iin_animation;
        prev = iprev;
        next = inext;
        current_time = 0.0f;
        max_time = imax_time;
    }

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

struct smooth_linear_interpolation_v3_t {
    bool in_animation;

    vector3_t current;
    vector3_t prev;
    vector3_t next;
    float current_time;
    float max_time;

    void set(
        bool iin_animation,
        vector3_t iprev,
        vector3_t inext,
        float imax_time) {
        in_animation = iin_animation;
        prev = iprev;
        next = inext;
        current_time = 0.0f;
        max_time = imax_time;
    }

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

// Starts fast, then slows down - this name is utter BS - I don't know the mathy name for it, it just looks like an exponential function
struct smooth_exponential_interpolation_t {
    bool in_animation;
    
    float destination;
    float current;
    float speed = 1.0f;

    void set(
        bool iin_animation,
        float idest,
        float icurrent,
        float ispeed = 1.0f) {
        in_animation = iin_animation;
        destination = idest;
        current = icurrent;
        speed = ispeed;
    }

    void animate(
        float dt) {
        if (in_animation) {
            current += (destination - current) * dt * speed;
            if (glm::abs(current - destination) < 0.001f) {
                in_animation = 0;
                current = destination;
            }
        }
    }
};

struct movement_axes_t {
    vector3_t right;
    vector3_t up;
    vector3_t forward;
};

inline movement_axes_t compute_movement_axes(
    const vector3_t &view_direction,
    const vector3_t &up) {
    vector3_t right = glm::normalize(glm::cross(view_direction, up));
    vector3_t forward = glm::normalize(glm::cross(up, right));
    movement_axes_t axes = {right, up, forward};
    return axes;
}
