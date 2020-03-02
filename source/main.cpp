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

static void s_imgui_test() {
    ImGui::Begin("General");

    static float rotation = 0.0;
    ImGui::SliderFloat("rotation", &rotation, 0.0f, 2.0f * 3.1415f);
    static float translation[] = { 0.0f, 0.0f };
    ImGui::SliderFloat2("position", translation, -1.0f, 1.0f);
    static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("color", color);

    if (ImGui::Button("Save")) {
        printf("Pressed save\n");
    }
    char buffer[100] = {};
    ImGui::InputText("string", buffer, IM_ARRAYSIZE(buffer));

    ImGui::End();
}

// TODO: Temporary - until engine is implemented
#include "r_internal.hpp"

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

    double now = glfwGetTime();
    float dt = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        r_camera_handle_input(dt, window);
        
        VkCommandBuffer command_buffer = begin_frame();

        r_camera_gpu_sync(command_buffer);

        begin_scene_rendering(command_buffer);

        end_scene_rendering(command_buffer);

        end_frame();

        double new_now = glfwGetTime();
        dt = (float)(new_now - now);
        now = new_now;
    }

    return 0;
}
