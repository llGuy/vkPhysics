#include "vk_scene3d_lighting.hpp"

namespace vk {

void shadow_box_t::init(const vector3_t &light_dir, const vector3_t &up, float far) {
    view = glm::lookAt(vector3_t(0.0f), light_dir, vector3_t(0.0f, 1.0f, 0.0f));
    inverse_view = glm::inverse(view);
    near = 1.0f;
    far = 25.0f;
}

void shadow_box_t::update(
    const vector3_t &light_dir,
    float fov,
    float aspect,
    const vector3_t &ws_position,
    const vector3_t &ws_direction,
    const vector3_t &ws_up) {
    view = glm::lookAt(vector3_t(0.0f), light_dir, vector3_t(0.0f, 1.0f, 0.0f));
    
    float far = far;
    float near = near;
    float far_width, near_width, far_height, near_height;
    
    far_width = 2.0f * far * tan(fov);
    near_width = 2.0f * near * tan(fov);
    far_height = far_width / aspect;
    near_height = near_width / aspect;

    vector3_t center_near = ws_position + ws_direction * near;
    vector3_t center_far = ws_position + ws_direction * far;
    
    vector3_t right_view_ax = glm::normalize(glm::cross(ws_direction, ws_up));
    vector3_t up_view_ax = -glm::normalize(glm::cross(ws_direction, right_view_ax));

    float far_width_half = far_width / 2.0f;
    float near_width_half = near_width / 2.0f;
    float far_height_half = far_height / 2.0f;
    float near_height_half = near_height / 2.0f;

    // f = far, n = near, l = left, r = right, t = top, b = bottom
    enum ortho_corner_t : int32_t {
	flt, flb,
	frt, frb,
	nlt, nlb,
	nrt, nrb
    };    

    // Light space
    ls_corners[flt] = view * vector4_t(ws_position + ws_direction * far - right_view_ax * far_width_half + up_view_ax * far_height_half, 1.0f);
    ls_corners[flb] = view * vector4_t(ws_position + ws_direction * far - right_view_ax * far_width_half - up_view_ax * far_height_half, 1.0f);
    
    ls_corners[frt] = view * vector4_t(ws_position + ws_direction * far + right_view_ax * far_width_half + up_view_ax * far_height_half, 1.0f);
    ls_corners[frb] = view * vector4_t(ws_position + ws_direction * far + right_view_ax * far_width_half - up_view_ax * far_height_half, 1.0f);
    
    ls_corners[nlt] = view * vector4_t(ws_position + ws_direction * near - right_view_ax * near_width_half + up_view_ax * near_height_half, 1.0f);
    ls_corners[nlb] = view * vector4_t(ws_position + ws_direction * near - right_view_ax * near_width_half - up_view_ax * near_height_half, 1.0f);
    
    ls_corners[nrt] = view * vector4_t(ws_position + ws_direction * near + right_view_ax * near_width_half + up_view_ax * near_height_half, 1.0f);
    ls_corners[nrb] = view * vector4_t(ws_position + ws_direction * near + right_view_ax * near_width_half - up_view_ax * near_height_half, 1.0f);

    x_min = x_max = ls_corners[0].x;
    y_min = y_max = ls_corners[0].y;
    z_min = z_max = ls_corners[0].z;

    for (uint32_t i = 1; i < 8; ++i) {
	if (x_min > ls_corners[i].x) x_min = ls_corners[i].x;
	if (x_max < ls_corners[i].x) x_max = ls_corners[i].x;

	if (y_min > ls_corners[i].y) y_min = ls_corners[i].y;
	if (y_max < ls_corners[i].y) y_max = ls_corners[i].y;

	if (z_min > ls_corners[i].z) z_min = ls_corners[i].z;
	if (z_max < ls_corners[i].z) z_max = ls_corners[i].z;
    }
    
    z_min = z_min - (z_max - z_min);

    projection = glm::transpose(
        matrix4_t(
            2.0f / (x_max - x_min), 0.0f, 0.0f, -(x_max + x_min) / (x_max - x_min),
            0.0f, 2.0f / (y_max - y_min), 0.0f, -(y_max + y_min) / (y_max - y_min),
            0.0f, 0.0f, 2.0f / (z_max - z_min), -(z_max + z_min) / (z_max - z_min),
            0.0f, 0.0f, 0.0f, 1.0f));
}

}
