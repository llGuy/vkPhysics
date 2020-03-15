#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "renderer.hpp"

#include <imgui.h>

#include <unistd.h>

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

    float eye_height = 0.5f;
    ImGui::SliderFloat("Eye height", &eye_height, 0.0f, 1.0f);

    float light_direction[3] = { 0.0f, 1.0f, 0.0f };
    ImGui::SliderFloat3("Light direction", light_direction, -1.0f, +1.0f);

    float rayleigh = -0.01f;
    ImGui::SliderFloat("Rayleigh factor", &rayleigh, -0.1f, 0.0f);
    
    float mie = -0.75f;
    ImGui::SliderFloat("Mie factor", &mie, -0.999f, -0.75f);

    float intensity = 1.5f;
    ImGui::SliderFloat("Intensity", &intensity, 0.1f, 30.0f);

    float scatter_strength = 19.0f;
    ImGui::SliderFloat("Scatter strength", &scatter_strength, 1.0f, 30.0f);

    float rayleigh_strength = 1.0f;
    ImGui::SliderFloat("Rayleigh strength", &rayleigh_strength, 0.0f, 3.0f);

    float mie_strength = 1.0f;
    ImGui::SliderFloat("Mie strength", &mie_strength, 0.0f, 3.0f);

    float rayleigh_collection = 1.0f;
    ImGui::SliderFloat("Rayleigh collection", &rayleigh_collection, 0.0f, 3.0f);

    float mie_collection = 1.0f;
    ImGui::SliderFloat("Mie collection", &mie_collection, 0.0f, 3.0f);

    base_cubemap_render_data_t *ptr = r_cubemap_render_data();

    /*ptr->eye_height = eye_height;
    ptr->light_direction.x = light_direction[0];
    ptr->light_direction.y = light_direction[1];
    ptr->light_direction.z = light_direction[2];
    ptr->light_direction.w = 1.0f;
    ptr->rayleigh = rayleigh;
    ptr->mie = mie;
    ptr->intensity = intensity;
    ptr->scatter_strength = scatter_strength;
    ptr->rayleigh_strength = rayleigh_strength;
    ptr->mie_strength = mie_strength;
    ptr->rayleigh_collection = rayleigh_collection;
    ptr->mie_collection = mie_collection;*/
    
    

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
    shader_binding_info_t sphere_info = {};
    
    load_mesh_internal(
        IM_SPHERE,
        &sphere,
        &sphere_info);

    const char *paths[] = { "../shaders/SPV/mesh.vert.spv", "../shaders/SPV/mesh.frag.spv" };
    shader_t sphere_shader = create_mesh_shader(
        &sphere_info,
        paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    mesh_render_data_t render_data = {};
    render_data.model = matrix4_t(1.0f);
    render_data.color = vector4_t(0.0f);
    render_data.pbr_info.x = 0.2f;
    render_data.pbr_info.y = 0.8;
    
    double now = glfwGetTime();
    float dt = 0.0f;

    float frame_time_max = 1.0f / 60.0f;

    printf("%i\n", sizeof(base_cubemap_render_data_t));
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        //r_render_environment_to_offscreen();
        
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
                xy *= 2.5f;

                vector3_t ws_position = vector3_t(xy, xy.x);

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

        /*if (dt < frame_time_max) {
            usleep((frame_time_max - dt) * 1000000.0f);
        }

        dt = frame_time_max;*/
        
        now = new_now;
    }

    return 0;
}
