#pragma once

#include <common/math.hpp>

namespace vk {

// This gets sent to the GPU when rendering 3D objects
struct camera_ubo_data_t {
    matrix4_t projection;
    matrix4_t view;
    matrix4_t inverse_view;
    matrix4_t view_projection;
    vector4_t frustum;
    vector4_t view_direction;
    matrix4_t previous_view_projection;
    float dt;
    float width;
    float height;
};

struct camera_info_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    vector2_t mouse_position;
    // Degrees
    float fov;
    float near, far;
    matrix4_t previous_view_projection;
};

// The game will need to pass in one of these for 3D scene to work
struct eye_3d_info_t {
    vector3_t position;
    vector3_t direction;
    vector3_t up;
    float fov;
    float near;
    float far;
    float dt;
};


enum frustum_corner_t {
    FLT, FLB,
    FRT, FRB,
    NLT, NLB,
    NRT, NRB
};    

enum frustum_plane_t {
    NEAR, FAR,
    LEFT, RIGHT,
    TOP, BOTTOM
};

struct frustum_t {
    vector3_t vertex[8];
    plane_t planes[6];

    vector3_t position;
    vector3_t direction;
    vector3_t up;
    // Radians
    float fov;
    float aspect;
    float near;
    float far;

    void init(
        const vector3_t &p,
        const vector3_t &d,
        const vector3_t &u,
        float fov,
        float aspect,
        float near,
        float far);

    bool check_point(const vector3_t &point);
    bool check_cube(frustum_t *frustum, const vector3_t &point, float radius);
};

}
