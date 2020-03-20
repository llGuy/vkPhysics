#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "renderer.hpp"
#include "engine.hpp"

#include <imgui.h>

int32_t main(
    int32_t argc,
    char *argv[]) {
    game_init_data_t game_init_data = {};
    game_init_data.flags = GIF_WINDOWED | GIF_CLIENT;

    game_main(&game_init_data);

    return 0;
}
