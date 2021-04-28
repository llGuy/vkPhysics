#include "vk_scene3d_camera.hpp"

namespace vk {

void frustum_t::init(
    const vector3_t &p,
    const vector3_t &d,
    const vector3_t &u,
    float f_fov,
    float f_aspect,
    float f_near,
    float f_far) {
    position = p;
    direction = d;

    vector3_t right = glm::normalize(glm::cross(d, u));

    up = glm::normalize(glm::cross(right, direction));
    fov = f_fov;
    near = f_near;
    far = f_far;

    float far_width_half = far * tan(fov);
    float near_width_half = near * tan(fov);
    float far_height_half = far_width_half / aspect;
    float near_height_half = near_width_half / aspect;

    vertex[FLT] = p + d * far - right * far_width_half + up * far_height_half;
    vertex[FLB] = p + d * far - right * far_width_half - up * far_height_half;
    vertex[FRT] = p + d * far + right * far_width_half + up * far_height_half;
    vertex[FRB] = p + d * far + right * far_width_half - up * far_height_half;

    vertex[NLT] = p + d * near - right * near_width_half + up * near_height_half;
    vertex[NLB] = p + d * near - right * near_width_half - up * near_height_half;
    vertex[NRT] = p + d * near + right * near_width_half + up * near_height_half;
    vertex[NRB] = p + d * near + right * near_width_half - up * near_height_half;

    planes[NEAR].point = vertex[NRB];
    planes[NEAR].normal = d;
    planes[NEAR].plane_constant = compute_plane_constant(&planes[NEAR]);

    planes[FAR].point = vertex[FLT];
    planes[FAR].normal = -d;
    planes[FAR].plane_constant = compute_plane_constant(&planes[FAR]);

    planes[LEFT].point = vertex[FLT];
    planes[LEFT].normal =
        glm::cross(
            glm::normalize(vertex[FLT] - vertex[FLB]),
            glm::normalize(vertex[NLB] - vertex[FLB]));
    planes[LEFT].plane_constant = compute_plane_constant(&planes[LEFT]);

    planes[RIGHT].point = vertex[FRT];
    planes[RIGHT].normal =
        -glm::cross(
            glm::normalize(vertex[FRT] - vertex[FRB]),
            glm::normalize(vertex[NRB] - vertex[FRB]));
    planes[RIGHT].plane_constant = compute_plane_constant(&planes[RIGHT]);

    planes[BOTTOM].point = vertex[FLB];
    planes[BOTTOM].normal =
        glm::cross(
            glm::normalize(vertex[FRB] - vertex[NRB]),
            glm::normalize(vertex[NLB] - vertex[NRB]));
    planes[BOTTOM].plane_constant = compute_plane_constant(&planes[BOTTOM]);

    planes[TOP].point = vertex[FLT];
    planes[TOP].normal =
        -glm::cross(
            glm::normalize(vertex[FRT] - vertex[NRT]),
            glm::normalize(vertex[NLT] - vertex[NRT]));
    planes[TOP].plane_constant = compute_plane_constant(&planes[TOP]);
}

bool frustum_t::check_point(const vector3_t &point) {
    for (uint32_t i = 0; i < 6; ++i) {
        float point_dot_normal = glm::dot(point, planes[i].normal);
        if (point_dot_normal + planes[i].plane_constant < 0.0f) {
            return false;
        }
    }

    return true;
}

bool frustum_t::check_cube(frustum_t *frustum, const vector3_t &point, float radius) {
    // Disabling for now
    return 1;

    static vector3_t cube_vertices[8] = {};

    cube_vertices[0] = point + vector3_t(-radius, -radius, -radius);
    cube_vertices[1] = point + vector3_t(-radius, -radius, +radius);
    cube_vertices[2] = point + vector3_t(+radius, -radius, +radius);
    cube_vertices[3] = point + vector3_t(+radius, -radius, -radius);

    cube_vertices[4] = point + vector3_t(-radius, +radius, -radius);
    cube_vertices[5] = point + vector3_t(-radius, +radius, +radius);
    cube_vertices[6] = point + vector3_t(+radius, +radius, +radius);
    cube_vertices[7] = point + vector3_t(+radius, +radius, -radius);

    for (uint32_t p = 0; p < 6; ++p) {
        bool cube_is_inside = false;

        for (uint32_t v = 0; v < 8; ++v) {
            if (glm::dot(cube_vertices[v], planes[p].normal) + planes[p].plane_constant >= 0.0f) {
                cube_is_inside = true;
                break;
            }
        }

        if (!cube_is_inside) {
            return false;
        }
    }

    return true;
}

}
