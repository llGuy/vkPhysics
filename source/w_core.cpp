#include "world.hpp"
#include "renderer.hpp"

static mesh_t cube = {};
static shader_t cube_shader;
static mesh_render_data_t cube_data = {};

static mesh_t sphere = {};
static shader_t sphere_shader;
static mesh_render_data_t render_data = {};

void world_init() {
    shader_binding_info_t cube_info = {};
    load_mesh_internal(IM_CUBE, &cube, &cube_info);

    const char *cube_paths[] = { "../shaders/SPV/untextured_mesh.vert.spv", "../shaders/SPV/untextured_mesh.geom.spv", "../shaders/SPV/untextured_mesh.frag.spv" };
    const char *cube_shadow_paths[] = { "../shaders/SPV/untextured_mesh_shadow.vert.spv", "../shaders/SPV/shadow.frag.spv" };    

    cube_shader = create_mesh_shader_color(&cube_info, cube_paths, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
    cube_data.model = glm::scale(vector3_t(20.0f, 0.3f, 20.0f));
    cube_data.color = vector4_t(1.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    
    // Temporary stuff, just to test lighing
    shader_binding_info_t sphere_info = {};
    
    load_mesh_internal(
        IM_SPHERE,
        &sphere,
        &sphere_info);

    const char *paths[] = { "../shaders/SPV/mesh.vert.spv", "../shaders/SPV/mesh.frag.spv" };
    const char *shadow_paths[] = { "../shaders/SPV/mesh_shadow.vert.spv", "../shaders/SPV/shadow.frag.spv" };
    sphere_shader = create_mesh_shader_color(
        &sphere_info,
        paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    render_data.model = matrix4_t(1.0f);
    render_data.color = vector4_t(1.0f);
    render_data.pbr_info.x = 0.2f;
    render_data.pbr_info.y = 0.8;
}

#include "r_internal.hpp"

void tick_world(
    VkCommandBuffer command_buffer) {
    r_camera_handle_input();
    r_update_lighting();

    cube_data.model = glm::scale(vector3_t(20.0f, 0.3f, 20.0f));
    cube_data.color = vector4_t(0.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(command_buffer, &cube, &cube_shader, &cube_data);

    cube_data.model = glm::translate(vector3_t(15.0f, 0.0f, 0.0f)) * glm::rotate(glm::radians(60.0f), vector3_t(1.0f, 1.0f, 0.0f)) * glm::scale(vector3_t(2.0f, 10.0f, 1.0f));
    cube_data.color = vector4_t(1.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(command_buffer, &cube, &cube_shader, &cube_data);

    for (uint32_t x = 0; x < 7; ++x) {
        for (uint32_t y = 0; y < 7; ++y) {
            vector2_t xz = vector2_t((float)x, (float)y);

            xz -= vector2_t(3.5f);
            xz *= 3.5f;

            vector3_t ws_position = vector3_t(xz.x, 1.25f, xz.y);

            render_data.model = glm::translate(ws_position);
            render_data.pbr_info.x = glm::clamp((float)x / 7.0f, 0.05f, 1.0f);
            render_data.pbr_info.y = (float)y / 7.0f;
                
            submit_mesh(command_buffer, &sphere, &sphere_shader, &render_data);
        }
    }

    r_render_environment(command_buffer);
}
