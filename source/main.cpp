#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "renderer.hpp"

#include <imgui.h>

static void s_create_vulkan_surface_proc(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window) {
    if (glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, surface) != VK_SUCCESS) {
        printf("Failed to create surface\n");
        exit(1);
    }
}

// TODO: Temporary - until engine is implemented
#include "r_internal.hpp"

static void s_imgui_test() {
    ImGui::Begin("General");

    ImGui::Text("Framerate: %.1f", ImGui::GetIO().Framerate);

    cpu_camera_data_t *camera_data = r_cpu_camera_data();

    ImGui::Text("Position: %.1f %.1f %.1f", camera_data->position.x, camera_data->position.y, camera_data->position.z);

    /*static float rotation = 0.0;
    ImGui::SliderFloat("rotation", &rotation, 0.0f, 2.0f * 3.1415f);
    static float translation[] = { 0.0f, 0.0f };
    ImGui::SliderFloat2("position", translation, -1.0f, 1.0f);
    static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("color", color);

    if (ImGui::Button("Save")) {
        printf("Pressed save\n");
    }
    char buffer[100] = {};
    ImGui::InputText("string", buffer, IM_ARRAYSIZE(buffer));*/

    ImGui::End();
}

int main(int argc, char *argv[]) {
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    GLFWwindow *window = glfwCreateWindow(vidmode->width / 2, vidmode->height / 2, "vkPhysics", NULL, NULL);

    int32_t width, height;
    glfwGetFramebufferSize(window, &width, &height);

    renderer_init("vkPhysics", &s_create_vulkan_surface_proc, &s_imgui_test, window, (uint32_t)width, (uint32_t)height);

    // Temporary stuff, just to test lighing
    mesh_t sphere = {};
    mesh_binding_info_t sphere_info = {};
    
    load_mesh_internal(
        IM_SPHERE,
        &sphere,
        &sphere_info);

    const char *paths[] = { "../shaders/SPV/mesh.vert.spv", "../shaders/SPV/mesh.frag.spv" };
    mesh_shader_t sphere_shader = create_mesh_shader(
        &sphere_info,
        paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    mesh_render_data_t render_data = {};
    render_data.model = matrix4_t(1.0f);
    render_data.color = vector4_t(0.5f, 0.0f ,0.0f, 1.0f);
    render_data.pbr_info.x = 0.2f;
    render_data.pbr_info.y = 0.8;
    
    double now = glfwGetTime();
    float dt = 0.0f;
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        r_camera_handle_input(dt, window);
 
        r_update_lighting();
        
        VkCommandBuffer command_buffer = begin_frame();

        r_camera_gpu_sync(command_buffer);
        r_lighting_gpu_sync(command_buffer);

        begin_scene_rendering(command_buffer);

        for (uint32_t x = 0; x < 7; ++x) {
            for (uint32_t y = 0; y < 7; ++y) {
                vector2_t xy = vector2_t((float)x, (float)y);

                xy -= vector2_t(3.5f);
                xy *= 3.0f;

                vector3_t ws_position = vector3_t(xy, 0.0f);

                render_data.model = glm::translate(ws_position);
                render_data.pbr_info.x = glm::clamp((float)x / 7.0f, 0.05f, 1.0f);
                render_data.pbr_info.y = (float)y / 7.0f;
                
                submit_mesh(command_buffer, &sphere, &sphere_shader, &render_data);
            }
        }

        r_render_environment(command_buffer);
        
        end_scene_rendering(command_buffer);

        end_frame();

        double new_now = glfwGetTime();
        dt = (float)(new_now - now);
        now = new_now;
    }

    return 0;
}
