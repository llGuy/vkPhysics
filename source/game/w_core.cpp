#include "world.hpp"
#include "engine.hpp"
#include "w_internal.hpp"
#include "renderer/input.hpp"
#include "renderer/renderer.hpp"

static mesh_t cube = {};
static shader_t cube_shader;
static mesh_render_data_t cube_data = {};

static mesh_t sphere = {};
static shader_t sphere_shader;
static mesh_render_data_t render_data = {};

// Temporary
static struct player_info_t {
    vector3_t position;
    vector3_t direction;
} player;

static chunk_world_t world;

void world_init() {
#if 0
    shader_binding_info_t cube_info = {};
    load_mesh_internal(IM_CUBE, &cube, &cube_info);

    const char *cube_paths[] = {
        "../shaders/SPV/untextured_mesh.vert.spv",
        "../shaders/SPV/untextured_mesh.geom.spv",
        "../shaders/SPV/untextured_mesh.frag.spv" };
    const char *cube_shadow_paths[] = {
        "../shaders/SPV/untextured_mesh_shadow.vert.spv",
        "../shaders/SPV/shadow.frag.spv" };    

    cube_shader = create_mesh_shader_color(
        &cube_info,
        cube_paths,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
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

    free_mesh_binding_info(&cube_info);
    free_mesh_binding_info(&sphere_info);
#endif
#if 1
    w_chunk_data_init();

    w_chunk_world_init(&world, 4);
#endif

    player.position = vector3_t(0.0f);
    player.direction = vector3_t(1.0f, 0.0f, 0.0f);
}

void destroy_world() {
    w_destroy_chunk_world(&world);
    w_destroy_chunk_data();
}

void handle_world_input() {
    game_input_t *game_input = get_game_input();
    raw_input_t *raw_input = get_raw_input();

    if (raw_input->buttons[BT_MOUSE_MIDDLE].state == BS_DOWN) {
        disable_cursor_display();
        vector3_t right = glm::normalize(glm::cross(player.direction, vector3_t(0.0f, 1.0f, 0.0f)));
    
        if (game_input->actions[GIAT_MOVE_FORWARD].state == BS_DOWN) {
            player.position += player.direction * surface_delta_time() * 10.0f;
        }
    
        if (game_input->actions[GIAT_MOVE_LEFT].state == BS_DOWN) {
            player.position -= right * surface_delta_time() * 10.0f;
        }
    
        if (game_input->actions[GIAT_MOVE_BACK].state == BS_DOWN) {
            player.position -= player.direction * surface_delta_time() * 10.0f;
        }
    
        if (game_input->actions[GIAT_MOVE_RIGHT].state == BS_DOWN) {
            player.position += right * surface_delta_time() * 10.0f;
        }
    
        if (game_input->actions[GIAT_TRIGGER4].state == BS_DOWN) { // Space
            player.position += vector3_t(0.0f, 1.0f, 0.0f) * surface_delta_time() * 10.0f;
        }
    
        if (game_input->actions[GIAT_TRIGGER6].state == BS_DOWN) { // Left shift
            player.position -= vector3_t(0.0f, 1.0f, 0.0f) * surface_delta_time() * 10.0f;
        }

        vector2_t new_mouse_position = vector2_t((float)game_input->mouse_x, (float)game_input->mouse_y);
        vector2_t delta = new_mouse_position - vector2_t(game_input->previous_mouse_x, game_input->previous_mouse_y);
    
        static constexpr float SENSITIVITY = 30.0f;
    
        vector3_t res = player.direction;
	    
        float x_angle = glm::radians(-delta.x) * SENSITIVITY * surface_delta_time();// *elapsed;
        float y_angle = glm::radians(-delta.y) * SENSITIVITY * surface_delta_time();// *elapsed;
                
        res = matrix3_t(glm::rotate(x_angle, vector3_t(0.0f, 1.0f, 0.0f))) * res;
        vector3_t rotate_y = glm::cross(res, vector3_t(0.0f, 1.0f, 0.0f));
        res = matrix3_t(glm::rotate(y_angle, rotate_y)) * res;

        res = glm::normalize(res);
                
        player.direction = res;
    }
    else {
        enable_cursor_display();
    }
}

void tick_world(
    VkCommandBuffer render_command_buffer,
    VkCommandBuffer transfer_command_buffer) {
#if 1
    w_chunk_gpu_sync_and_render(
        render_command_buffer,
        transfer_command_buffer,
        &world);
#endif
    
    if (render_command_buffer != VK_NULL_HANDLE) {
        render_environment(render_command_buffer);
    }

#if 0
    cube_data.model = glm::scale(vector3_t(20.0f, 0.3f, 20.0f));
    cube_data.color = vector4_t(0.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(render_command_buffer, &cube, &cube_shader, &cube_data);

    cube_data.model = glm::translate(vector3_t(15.0f, 0.0f, 0.0f)) * glm::rotate(glm::radians(60.0f), vector3_t(1.0f, 1.0f, 0.0f)) * glm::scale(vector3_t(2.0f, 10.0f, 1.0f));
    cube_data.color = vector4_t(1.0f);
    cube_data.pbr_info.x = 0.07f;
    cube_data.pbr_info.y = 0.1f;
    submit_mesh(render_command_buffer, &cube, &cube_shader, &cube_data);

    for (uint32_t x = 0; x < 7; ++x) {
        for (uint32_t y = 0; y < 7; ++y) {
            vector2_t xz = vector2_t((float)x, (float)y);

            xz -= vector2_t(3.5f);
            xz *= 3.5f;

            vector3_t ws_position = vector3_t(xz.x, 1.25f, xz.y);

            render_data.model = glm::translate(ws_position);
            render_data.pbr_info.x = glm::clamp((float)x / 7.0f, 0.05f, 1.0f);
            render_data.pbr_info.y = (float)y / 7.0f;
                
            submit_mesh(render_command_buffer, &sphere, &sphere_shader, &render_data);
        }
    }
#endif
}

eye_3d_info_t create_eye_info() {
    eye_3d_info_t info = {};

    info.position = player.position;
    info.direction = player.direction;
    info.up = vector3_t(0.0f, 1.0f, 0.0f);
    info.fov = 60.0f;
    info.near = 0.1f;
    info.far = 10000.0f;
    info.dt = surface_delta_time();

    return info;
}

lighting_info_t create_lighting_info() {
    lighting_info_t info = {};
    
    info.ws_directional_light = vector4_t(0.1f, 0.422f, 0.714f, 0.0f);
    info.lights_count = 0;

    return info;
}
