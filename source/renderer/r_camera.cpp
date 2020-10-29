#include "common/math.hpp"
#include "renderer.hpp"
#include <GLFW/glfw3.h>
#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include <common/tools.hpp>
#include <common/log.hpp>

static gpu_buffer_t transforms_uniform_buffer;
static VkDescriptorSet descriptor_set;
static gpu_camera_transforms_t transforms;

VkDescriptorSet r_camera_transforms_uniform() {
    return descriptor_set;
}

static cpu_camera_data_t camera_data;

gpu_camera_transforms_t *r_gpu_camera_data() {
    return &transforms;
}

cpu_camera_data_t *r_cpu_camera_data() {
    return &camera_data;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (camera_data.fov >= 1.0f && camera_data.fov <= 80.0f)
        camera_data.fov -= (float)yoffset;
    if (camera_data.fov <= 1.0f)
        camera_data.fov = 1.0f;
    if (camera_data.fov >= 80.0f)
        camera_data.fov = 45.0f;
}

void r_camera_init(void *window) {
    glfwSetScrollCallback((GLFWwindow *)window, scroll_callback);
    
    camera_data.position = vector3_t(0.0f, 5.0f, 0.0f);
    camera_data.direction = vector3_t(1.0f, 0.0, 0.0f);
    camera_data.up = vector3_t(0.0f, 1.0f, 0.0f);
    camera_data.fov = 60.0f;
    camera_data.near = 0.1f;
    camera_data.far = 10000.0f;
    
    double x, y;
    glfwGetCursorPos((GLFWwindow *)window, &x, &y);
    camera_data.mouse_position = vector2_t((float)x, (float)y);

    VkExtent2D extent = r_swapchain_extent();
    transforms.projection = glm::perspective(glm::radians(camera_data.fov), (float)extent.width / (float)extent.height, camera_data.near, camera_data.far);
    transforms.projection[1][1] *= -1.0f;
    transforms.view = glm::lookAt(camera_data.position, camera_data.position + camera_data.direction, camera_data.up);
    transforms.inverse_view = glm::inverse(transforms.view);
    transforms.view_projection = transforms.projection * transforms.view;
    camera_data.previous_view_projection = transforms.previous_view_projection = transforms.view_projection * transforms.inverse_view;
    transforms.frustum.x = camera_data.near;
    transforms.frustum.y = camera_data.far;
    transforms.view_direction = vector4_t(camera_data.direction, 1.0f);
    transforms.dt = 1.0f / 60.0f;
    transforms.width = (float)extent.width;
    transforms.height = (float)extent.height;
    
    transforms_uniform_buffer = create_gpu_buffer(
        sizeof(gpu_camera_transforms_t),
        &transforms,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    descriptor_set = create_buffer_descriptor_set(
        transforms_uniform_buffer.buffer,
        transforms_uniform_buffer.size,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

// Should not worry about case where not running in window mode (server mode) because anyway, r_ prefixed files won't be running
void r_camera_gpu_sync(
    VkCommandBuffer command_buffer,
    eye_3d_info_t *eye_info) {
    transforms.dt = eye_info->dt;

    camera_data.position = eye_info->position;
    camera_data.direction = eye_info->direction;
    camera_data.up = eye_info->up;
    camera_data.fov = eye_info->fov;
    camera_data.near = eye_info->near;
    camera_data.far = eye_info->far;
    
    transforms.projection = glm::perspective(glm::radians(camera_data.fov), (float)r_swapchain_extent().width / (float)r_swapchain_extent().height, camera_data.near, camera_data.far);
    transforms.projection[1][1] *= -1.0f;
    transforms.view = glm::lookAt(camera_data.position, camera_data.position + camera_data.direction, camera_data.up);
    transforms.inverse_view = glm::inverse(transforms.view);
    transforms.view_projection = transforms.projection * transforms.view;
    VkExtent2D extent = r_swapchain_extent();
    transforms.width = (float)extent.width;
    transforms.height = (float)extent.height;
    transforms.previous_view_projection = transforms.previous_view_projection * transforms.inverse_view;

    VkBufferMemoryBarrier buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        &transforms_uniform_buffer,
        0,
        (uint32_t)transforms_uniform_buffer.size);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);
    
    vkCmdUpdateBuffer(
        command_buffer,
        transforms_uniform_buffer.buffer,
        0,
        transforms_uniform_buffer.size,
        &transforms);

    buffer_barrier = create_gpu_buffer_barrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        &transforms_uniform_buffer,
        0,
        (uint32_t)transforms_uniform_buffer.size);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);

    transforms.previous_view_projection = transforms.view_projection;
}

void create_frustum(
    frustum_t *frustum,
    const vector3_t &p,
    const vector3_t &d,
    const vector3_t &u,
    float fov,
    float aspect,
    float near,
    float far) {
    frustum->position = p;
    frustum->direction = d;

    vector3_t right = glm::normalize(glm::cross(d, u));

    frustum->up = glm::normalize(glm::cross(right, frustum->direction));
    frustum->fov = fov;
    frustum->near = near;
    frustum->far = far;

    float far_width_half = far * tan(fov);
    float near_width_half = near * tan(fov);
    float far_height_half = far_width_half / aspect;
    float near_height_half = near_width_half / aspect;

    frustum->vertex[FLT] = p + d * far - right * far_width_half + frustum->up * far_height_half;
    frustum->vertex[FLB] = p + d * far - right * far_width_half - frustum->up * far_height_half;
    frustum->vertex[FRT] = p + d * far + right * far_width_half + frustum->up * far_height_half;
    frustum->vertex[FRB] = p + d * far + right * far_width_half - frustum->up * far_height_half;

    frustum->vertex[NLT] = p + d * near - right * near_width_half + frustum->up * near_height_half;
    frustum->vertex[NLB] = p + d * near - right * near_width_half - frustum->up * near_height_half;
    frustum->vertex[NRT] = p + d * near + right * near_width_half + frustum->up * near_height_half;
    frustum->vertex[NRB] = p + d * near + right * near_width_half - frustum->up * near_height_half;

    frustum->planes[NEAR].point = frustum->vertex[NRB];
    frustum->planes[NEAR].normal = d;
    frustum->planes[NEAR].plane_constant = compute_plane_constant(&frustum->planes[NEAR]);

    frustum->planes[FAR].point = frustum->vertex[FLT];
    frustum->planes[FAR].normal = -d;
    frustum->planes[FAR].plane_constant = compute_plane_constant(&frustum->planes[FAR]);

    frustum->planes[LEFT].point = frustum->vertex[FLT];
    frustum->planes[LEFT].normal =
        glm::cross(
            glm::normalize(frustum->vertex[FLT] - frustum->vertex[FLB]),
            glm::normalize(frustum->vertex[NLB] - frustum->vertex[FLB]));
    frustum->planes[LEFT].plane_constant = compute_plane_constant(&frustum->planes[LEFT]);

    frustum->planes[RIGHT].point = frustum->vertex[FRT];
    frustum->planes[RIGHT].normal =
        -glm::cross(
            glm::normalize(frustum->vertex[FRT] - frustum->vertex[FRB]),
            glm::normalize(frustum->vertex[NRB] - frustum->vertex[FRB]));
    frustum->planes[RIGHT].plane_constant = compute_plane_constant(&frustum->planes[RIGHT]);

    frustum->planes[BOTTOM].point = frustum->vertex[FLB];
    frustum->planes[BOTTOM].normal =
        glm::cross(
            glm::normalize(frustum->vertex[FRB] - frustum->vertex[NRB]),
            glm::normalize(frustum->vertex[NLB] - frustum->vertex[NRB]));
    frustum->planes[BOTTOM].plane_constant = compute_plane_constant(&frustum->planes[BOTTOM]);

    frustum->planes[TOP].point = frustum->vertex[FLT];
    frustum->planes[TOP].normal =
        -glm::cross(
            glm::normalize(frustum->vertex[FRT] - frustum->vertex[NRT]),
            glm::normalize(frustum->vertex[NLT] - frustum->vertex[NRT]));
    frustum->planes[TOP].plane_constant = compute_plane_constant(&frustum->planes[TOP]);
}

bool frustum_check_point(
    frustum_t *frustum,
    const vector3_t &point) {
    for (uint32_t i = 0; i < 6; ++i) {
        float point_dot_normal = glm::dot(point, frustum->planes[i].normal);
        if (point_dot_normal + frustum->planes[i].plane_constant < 0.0f) {
            return false;
        }
    }

    return true;
}

bool frustum_check_cube(
    frustum_t *frustum,
    const vector3_t &point,
    float radius) {
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
            if (glm::dot(cube_vertices[v], frustum->planes[p].normal) + frustum->planes[p].plane_constant >= 0.0f) {
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
