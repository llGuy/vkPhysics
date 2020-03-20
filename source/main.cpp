#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "renderer.hpp"
#include "engine.hpp"

#include <imgui.h>

static void s_create_vulkan_surface_proc(struct VkInstance_T *instance, struct VkSurfaceKHR_T **surface, void *window) {
    if (glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, surface) != VK_SUCCESS) {
        printf("Failed to create surface\n");
        exit(1);
    }
}

// TODO: Temporary - until engine is implemented
#include "r_internal.hpp"

bool changed = 0;

static void s_window_resize_callback(GLFWwindow *window, int32_t width, int32_t height) {
    handle_resize(width, height);
}

bool is_fullscreen = 0;

static void s_window_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        if (is_fullscreen) {

        }
        else {
            const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());

            glfwSetWindowMonitor(window, glfwGetWindowMonitor(window), 0, 0, vidmode->width, vidmode->height, 0);
        }
    }
}

int32_t main(
    int32_t argc,
    char *argv[]) {
    game_init_data_t game_init_data = {};
    game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

    game_main(&game_init_data);

    return 0;
}
