#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "r_internal.hpp"
#include "renderer/renderer.hpp"

static lighting_data_t lighting_data;

static vector3_t light_direction;

vector3_t *r_light_direction() {
    return &light_direction;
}

static gpu_buffer_t light_uniform_buffer;
static VkDescriptorSet light_descriptor_set;

VkDescriptorSet r_lighting_uniform() {
    return light_descriptor_set;
}

struct shadow_box_t {
    matrix4_t view;
    matrix4_t projection;
    matrix4_t inverse_view;

    float near;
    float far;

    vector4_t ls_corners[8];
    
    union {
        struct {float x_min, x_max, y_min, y_max, z_min, z_max;};
        float corner_values[6];
    };
};

static shadow_box_t scene_shadow_box;

static void s_shadow_box_init() {
    // Replace up vector with actual camera up vector
    scene_shadow_box.view = glm::lookAt(vector3_t(0.0f), light_direction, vector3_t(0.0f, 1.0f, 0.0f));
    scene_shadow_box.inverse_view = glm::inverse(scene_shadow_box.view);
    scene_shadow_box.near = 1.0f;
    scene_shadow_box.far = 25.0f;
}

static void s_update_shadow_box(
    float fov,
    float aspect,
    const vector3_t &ws_position,
    const vector3_t &ws_direction,
    const vector3_t &ws_up,
    shadow_box_t *shadow_box) {
    scene_shadow_box.view = glm::lookAt(vector3_t(0.0f), light_direction, vector3_t(0.0f, 1.0f, 0.0f));
    
    float far = shadow_box->far;
    float near = shadow_box->near;
    float far_width, near_width, far_height, near_height;
    
    far_width = 2.0f * far * tan(fov);
    near_width = 2.0f * near * tan(fov);
    far_height = far_width / aspect;
    near_height = near_width / aspect;

    vector3_t center_near = ws_position + ws_direction * near;
    vector3_t center_far = ws_position + ws_direction * far;
    
    vector3_t right_view_ax = glm::normalize(glm::cross(ws_direction, ws_up));
    vector3_t up_view_ax = glm::normalize(glm::cross(ws_direction, right_view_ax));

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
    shadow_box->ls_corners[flt] = shadow_box->view * vector4_t(ws_position + ws_direction * far - right_view_ax * far_width_half + up_view_ax * far_height_half, 1.0f);
    shadow_box->ls_corners[flb] = shadow_box->view * vector4_t(ws_position + ws_direction * far - right_view_ax * far_width_half - up_view_ax * far_height_half, 1.0f);
    
    shadow_box->ls_corners[frt] = shadow_box->view * vector4_t(ws_position + ws_direction * far + right_view_ax * far_width_half + up_view_ax * far_height_half, 1.0f);
    shadow_box->ls_corners[frb] = shadow_box->view * vector4_t(ws_position + ws_direction * far + right_view_ax * far_width_half - up_view_ax * far_height_half, 1.0f);
    
    shadow_box->ls_corners[nlt] = shadow_box->view * vector4_t(ws_position + ws_direction * near - right_view_ax * near_width_half + up_view_ax * near_height_half, 1.0f);
    shadow_box->ls_corners[nlb] = shadow_box->view * vector4_t(ws_position + ws_direction * near - right_view_ax * near_width_half - up_view_ax * near_height_half, 1.0f);
    
    shadow_box->ls_corners[nrt] = shadow_box->view * vector4_t(ws_position + ws_direction * near + right_view_ax * near_width_half + up_view_ax * near_height_half, 1.0f);
    shadow_box->ls_corners[nrb] = shadow_box->view * vector4_t(ws_position + ws_direction * near + right_view_ax * near_width_half - up_view_ax * near_height_half, 1.0f);

    float x_min, x_max, y_min, y_max, z_min, z_max;

    x_min = x_max = shadow_box->ls_corners[0].x;
    y_min = y_max = shadow_box->ls_corners[0].y;
    z_min = z_max = shadow_box->ls_corners[0].z;

    for (uint32_t i = 1; i < 8; ++i) {
	if (x_min > shadow_box->ls_corners[i].x) x_min = shadow_box->ls_corners[i].x;
	if (x_max < shadow_box->ls_corners[i].x) x_max = shadow_box->ls_corners[i].x;

	if (y_min > shadow_box->ls_corners[i].y) y_min = shadow_box->ls_corners[i].y;
	if (y_max < shadow_box->ls_corners[i].y) y_max = shadow_box->ls_corners[i].y;

	if (z_min > shadow_box->ls_corners[i].z) z_min = shadow_box->ls_corners[i].z;
	if (z_max < shadow_box->ls_corners[i].z) z_max = shadow_box->ls_corners[i].z;
    }
    
    shadow_box->x_min = x_min = x_min;
    shadow_box->x_max = x_max = x_max;
    shadow_box->y_min = y_min = y_min;
    shadow_box->y_max = y_max = y_max;
    shadow_box->z_min = z_min = z_min;
    shadow_box->z_max = z_max = z_max;

    z_min = z_min - (z_max - z_min);

    shadow_box->projection = glm::transpose(
        matrix4_t(
            2.0f / (x_max - x_min), 0.0f, 0.0f, -(x_max + x_min) / (x_max - x_min),
            0.0f, 2.0f / (y_max - y_min), 0.0f, -(y_max + y_min) / (y_max - y_min),
            0.0f, 0.0f, 2.0f / (z_max - z_min), -(z_max + z_min) / (z_max - z_min),
            0.0f, 0.0f, 0.0f, 1.0f));
}

void r_lighting_init() {
    memset(&lighting_data, 0, sizeof(lighting_data));

    gpu_camera_transforms_t *transforms = r_gpu_camera_data();

    light_direction = vector3_t(0.1f, 0.422f, 0.714f);

    lighting_data.ws_light_positions[0] = vector4_t(0.0f, 4.0f, 0.0f, 1.0f);
    lighting_data.light_positions[0] = transforms->view * lighting_data.ws_light_positions[0];

    lighting_data.ws_light_directions[0] = glm::normalize(vector4_t(0.0f, -0.3f, 0.8f, 0.0f));
    lighting_data.light_directions[0] = transforms->view * lighting_data.ws_light_directions[0];
    
    lighting_data.light_colors[0] = vector4_t(300.0f, 300.0f, 300.0f, 0.0f);
    
    lighting_data.ws_directional_light = -vector4_t(light_direction, 0.0f);
    lighting_data.vs_directional_light = transforms->view * lighting_data.ws_directional_light;

    lighting_data.point_light_count = 0;

    s_shadow_box_init();
    
    light_uniform_buffer = create_gpu_buffer(
        sizeof(lighting_data_t),
        &lighting_data,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    light_descriptor_set = create_buffer_descriptor_set(
        light_uniform_buffer.buffer,
        sizeof(lighting_data_t),
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void r_update_lighting(
    lighting_info_t *lighting,
    eye_3d_info_t *eye) {
    gpu_camera_transforms_t *transforms = r_gpu_camera_data();
    cpu_camera_data_t *camera_data = r_cpu_camera_data();

    for (uint32_t i = 0; i < lighting->lights_count; ++i) {
        lighting_data.ws_light_positions[i] = lighting->ws_light_positions[i];
        lighting_data.light_positions[i] = transforms->view * lighting->ws_light_positions[i];
        lighting_data.ws_light_directions[i] = lighting->ws_light_directions[i];
        lighting_data.light_directions[i] = transforms->view * lighting->ws_light_directions[i];
        lighting_data.light_colors[i] = lighting->light_colors[i];
    }

    lighting_data.point_light_count = lighting->lights_count;

    s_update_shadow_box(
        glm::radians(fmax(eye->fov, 60.0f)),
        transforms->width / transforms->height,
        camera_data->position,
        camera_data->direction,
        camera_data->up,
        &scene_shadow_box);

    matrix4_t view_rotation = transforms->view;
    view_rotation[3][0] = 0.0f;
    view_rotation[3][1] = 0.0f;
    view_rotation[3][2] = 0.0f;
    
    lighting_data.ws_directional_light = -glm::normalize(lighting->ws_directional_light);
    lighting_data.vs_directional_light = view_rotation * lighting_data.ws_directional_light;

    vector4_t light_position = vector4_t(light_direction * 1000000.0f, 0.0f);
    
    light_position = transforms->projection * view_rotation * light_position;
    light_position.x /= light_position.w;
    light_position.y /= light_position.w;
    light_position.z /= light_position.w;
    light_position.x = light_position.x * 0.5f + 0.5f;
    light_position.y = light_position.y * 0.5f + 0.5f;
    lighting_data.light_screen_coord = vector2_t(light_position.x, light_position.y);

    lighting_data.shadow_view = scene_shadow_box.view;
    lighting_data.shadow_projection = scene_shadow_box.projection;
    lighting_data.shadow_view_projection = scene_shadow_box.projection * scene_shadow_box.view;
}

void r_lighting_gpu_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye,
    lighting_info_t *lighting) {
    r_update_lighting(lighting, eye);
    
    VkBufferMemoryBarrier barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        &light_uniform_buffer,
        0,
        sizeof(lighting_data_t));

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);

    vkCmdUpdateBuffer(
        command_buffer,
        light_uniform_buffer.buffer,
        0,
        sizeof(lighting_data_t),
        &lighting_data);

    barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        &light_uniform_buffer,
        0,
        sizeof(lighting_data_t));

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL);
}
