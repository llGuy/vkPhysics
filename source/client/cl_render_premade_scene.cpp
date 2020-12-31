#include "cl_render.hpp"

namespace cl {

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

    vector3_t *vertices = LN_MALLOC(vector3_t, vertex_count);

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
    VkCommandBuffer transfer,
    fixed_premade_scene_t *scene) {
    vk::begin_mesh_submission(
        render,
        dr_get_shader_rsc(GS_BALL),
        dr_chunk_colors_g.chunk_color_set);

    scene->world_render_data.color = vector4_t(0.0f);

    vk::submit_mesh(
        render,
        &scene->world_mesh,
        dr_get_shader_rsc(GS_BALL),
        {&scene->world_render_data, vk::DEF_MESH_RENDER_DATA_SIZE});
}

}
