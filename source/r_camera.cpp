#include "tools.hpp"
#include "renderer.hpp"
#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

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
    if (camera_data.fov >= 1.0f && camera_data.fov <= 90.0f)
        camera_data.fov -= yoffset;
    if (camera_data.fov <= 1.0f)
        camera_data.fov = 1.0f;
    if (camera_data.fov >= 90.0f)
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
void r_camera_gpu_sync(VkCommandBuffer command_buffer) {
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
        transforms_uniform_buffer.size);

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
        transforms_uniform_buffer.size);

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0,
        0, NULL,
        1, &buffer_barrier,
        0, NULL);
}

// TODO: Add proper input handling (this is a placeholder until proper world / entity infrastructure is added)
void r_camera_handle_input(float dt, void *window) {
    transforms.previous_view_projection = transforms.view_projection;
    transforms.dt = dt;
    
    GLFWwindow *glfw_window = (GLFWwindow *)window;

    if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_MIDDLE)) {
        glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        vector3_t right = glm::normalize(glm::cross(camera_data.direction, camera_data.up));
    
        if (glfwGetKey(glfw_window, GLFW_KEY_W)) {
            camera_data.position += camera_data.direction * dt * 10.0f;
        }
    
        if (glfwGetKey(glfw_window, GLFW_KEY_A)) {
            camera_data.position -= right * dt * 10.0f;
        }
    
        if (glfwGetKey(glfw_window, GLFW_KEY_S)) {
            camera_data.position -= camera_data.direction * dt * 10.0f;
        }
    
        if (glfwGetKey(glfw_window, GLFW_KEY_D)) {
            camera_data.position += right * dt * 10.0f;
        }

        if (glfwGetKey(glfw_window, GLFW_KEY_SPACE)) {
            camera_data.position += camera_data.up * dt * 10.0f;
        }

        if (glfwGetKey(glfw_window, GLFW_KEY_LEFT_SHIFT)) {
            camera_data.position -= camera_data.up * dt * 10.0f;
        }

        double x, y;
        glfwGetCursorPos((GLFWwindow *)window, &x, &y);
        vector2_t new_mouse_position = vector2_t((float)x, (float)y);
        vector2_t delta = new_mouse_position - camera_data.mouse_position;
    
        static constexpr float SENSITIVITY = 30.0f;
    
        vector3_t res = camera_data.direction;
	    
        float x_angle = glm::radians(-delta.x) * SENSITIVITY * dt;// *elapsed;
        float y_angle = glm::radians(-delta.y) * SENSITIVITY * dt;// *elapsed;
                
        res = matrix3_t(glm::rotate(x_angle, camera_data.up)) * res;
        vector3_t rotate_y = glm::cross(res, camera_data.up);
        res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

        res = glm::normalize(res);
                
        camera_data.direction = res;

        transforms.view_direction = vector4_t(camera_data.direction, 1.0f);

        camera_data.mouse_position = new_mouse_position;
    }
    else {
        glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        double x, y;
        glfwGetCursorPos((GLFWwindow *)window, &x, &y);
        vector2_t new_mouse_position = vector2_t((float)x, (float)y);
        camera_data.mouse_position = new_mouse_position;
    }
}
