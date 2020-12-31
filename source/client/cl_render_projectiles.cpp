#include "cl_render.hpp"

namespace cl {

static vk::mesh_t bullet_mesh;
static object_shaders_t bullet_shaders;

void init_projectile_render_resources() {
    vk::shader_binding_info_t bullet_sbi = {};
    bullet_mesh.load_external(&bullet_sbi, "assets/models/bullet.mesh");

    { // Create shaders for the bullet mesh
        vk::shader_create_info_t sc_info;

        const char *static_shader_paths[] = {
            "shaders/SPV/mesh.vert.spv", "shaders/SPV/mesh.geom.spv", "shaders/SPV/mesh.frag.spv"
        };

        const char *static_shadow_shader_paths[] = {
            "shaders/SPV/mesh_shadow.vert.spv", "shaders/SPV/shadow.frag.spv"
        };

        sc_info.binding_info = &bullet_sbi;
        sc_info.shader_paths = static_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sc_info.cull_mode = VK_CULL_MODE_NONE;
        sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        bullet_shaders.albedo = vk::create_mesh_shader_color(&sc_info, vk::MT_STATIC);

        sc_info.shader_paths = static_shadow_shader_paths;
        sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        bullet_shaders.shadow = vk::create_mesh_shader_shadow(&sc_info, vk::MT_STATIC);
    }
}

void projectiles_gpu_sync_and_render(
    VkCommandBuffer render,
    VkCommandBuffer shadow,
    VkCommandBuffer transfer,
    vkph::state_t *state) {
    for (uint32_t i = 0; i < state->rocks.list.data_count; ++i) {
        vk::mesh_render_data_t data = {};
        data.color = vector4_t(0.0f);
        data.model = glm::translate(state->rocks.list[i].position) * glm::scale(vector3_t(0.2f));
        data.pbr_info.x = 0.5f;
        data.pbr_info.y = 0.5f;

        vk::begin_mesh_submission(render, &bullet_shaders.albedo);

        vk::submit_mesh(
            render,
            &bullet_mesh,
            &bullet_shaders.albedo,
            {&data, vk::DEF_MESH_RENDER_DATA_SIZE});

        vk::submit_mesh_shadow(
            shadow,
            &bullet_mesh,
            &bullet_shaders.shadow,
            {&data, vk::DEF_MESH_RENDER_DATA_SIZE});
    }
}

}
