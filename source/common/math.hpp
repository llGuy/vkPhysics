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

template <typename T>
struct linear_interpolation_t {
    bool in_animation;
    T current;
    T prev;
    T next;
    float current_time;
    float max_time;

    // Doesn't matter where we are in the interpolation - just set these values
    void set(bool iin_animation, T iprev, T inext, float imax_time) {
        in_animation = iin_animation;
        prev = iprev;
        next = inext;
        current_time = 0.0f;
        max_time = imax_time;
    }

    void animate(float dt) {
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


using linear_interpolation_f32_t = linear_interpolation_t<float>;
using linear_interpolation_v3_t  = linear_interpolation_t<vector3_t>;
using linear_interpolation_v4_t  = linear_interpolation_t<vector4_t>;

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

inline float calculate_sphere_circumference(
    float radius) {
    return(2.0f * 3.1415f * radius);
}

struct plane_t {
    float plane_constant;
    vector3_t normal;
    vector3_t point;
};

inline float compute_plane_constant(
    plane_t *plane) {
    return -glm::dot(plane->point, plane->normal);
}

inline float compute_point_to_plane_dist(
    plane_t *plane,
    const vector3_t &point) {
    return glm::dot(point, plane->normal) + plane->plane_constant;
}

constexpr float PI = 3.14159265359f;
