#include "cl_render.hpp"

namespace cl {

static vk::shader_t premade_scene_shader;

void init_fixed_premade_scene_shaders() {
    vk::shader_binding_info_t binding_info = {};
    binding_info.binding_count = 1;
    binding_info.binding_descriptions = flmalloc<VkVertexInputBindingDescription>(binding_info.binding_count);

    binding_info.attribute_count = 1;
    binding_info.attribute_descriptions = flmalloc<VkVertexInputAttributeDescription>(binding_info.attribute_count);

    binding_info.binding_descriptions[0].binding = 0;
    binding_info.binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_info.binding_descriptions[0].stride = sizeof(vector3_t);
    
    binding_info.attribute_descriptions[0].binding = 0;
    binding_info.attribute_descriptions[0].location = 0;
    binding_info.attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    binding_info.attribute_descriptions[0].offset = 0;

    vk::shader_create_info_t sc_info;
    
    const char *static_shader_paths[] = {
        "shaders/SPV/mesh.vert.spv", "shaders/SPV/mesh.geom.spv", "shaders/SPV/mesh.frag.spv"
    };

    const char *static_shadow_shader_paths[] = {
        "shaders/SPV/mesh_shadow.vert.spv", "shaders/SPV/shadow.frag.spv"
    };

    sc_info.binding_info = &binding_info;
    sc_info.shader_paths = static_shader_paths;
    sc_info.shader_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    sc_info.cull_mode = VK_CULL_MODE_NONE;
    sc_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    premade_scene_shader= vk::create_mesh_shader_color(&sc_info, vk::MT_STATIC);
}

void fixed_premade_scene_t::load(const char *path) {
    file_handle_t input = create_file(path, FLF_BINARY);

    file_contents_t contents = read_file(
        input);

    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    vector3_t ws_position = serialiser.deserialise_vector3();
    vector3_t ws_view_direction = serialiser.deserialise_vector3();

    uint32_t vertex_count = serialiser.deserialise_uint32();

    vector3_t *vertices = lnmalloc<vector3_t>(vertex_count);

    for (uint32_t i = 0; i < vertex_count; ++i) {
        vertices[i] = serialiser.deserialise_vector3();
    }

    world_mesh.push_buffer(vk::BT_VERTEX);
    vk::mesh_buffer_t *vtx_buffer = world_mesh.get_mesh_buffer(vk::BT_VERTEX);
    vtx_buffer->gpu_buffer.init(
        sizeof(vector3_t) * vertex_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    world_mesh.vertex_offset = 0;
    world_mesh.vertex_count = vertex_count;
    world_mesh.first_index = 0;
    world_mesh.index_offset = 0;
    world_mesh.index_count = 0;
    world_mesh.index_type = VK_INDEX_TYPE_UINT32;

    world_mesh.init_mesh_vbo_final_list();

    world_render_data.model = matrix4_t(1.0f);
    world_render_data.pbr_info.x = 0.1f;
    world_render_data.pbr_info.y = 0.1f;
    world_render_data.color = vector4_t(0.0f);

    position = ws_position;
    view_direction = ws_view_direction;
    up_vector = vector3_t(0.0f, 1.0f, 0.0f);
}

void premade_scene_gpu_sync_and_render(
    VkCommandBuffer render,
    fixed_premade_scene_t *scene) {
    vk::begin_mesh_submission(render, &premade_scene_shader);

    scene->world_render_data.color = vector4_t(0.0f);

    vk::submit_mesh(
        render,
        &scene->world_mesh,
        &premade_scene_shader,
        {&scene->world_render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
}

}
