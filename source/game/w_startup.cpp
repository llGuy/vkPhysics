#include "w_internal.hpp"
#include <common/files.hpp>
#include <common/serialiser.hpp>
#include <renderer/renderer.hpp>

// At the startup screen, we render a sort of world
static startup_screen_t startup_screen;

startup_screen_t *get_startup_screen_data() {
    return &startup_screen;
}

void w_read_startup_screen(
    world_t *world) {
    file_handle_t input = create_file(
        "assets/misc/startup/default.startup",
        FLF_BINARY);

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

    push_buffer_to_mesh(BT_VERTEX, &startup_screen.world_mesh);
    mesh_buffer_t *vtx_buffer = get_mesh_buffer(BT_VERTEX, &startup_screen.world_mesh);
    vtx_buffer->gpu_buffer = create_gpu_buffer(
        sizeof(vector3_t) * vertex_count,
        vertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    startup_screen.world_mesh.vertex_offset = 0;
    startup_screen.world_mesh.vertex_count = vertex_count;
    startup_screen.world_mesh.first_index = 0;
    startup_screen.world_mesh.index_offset = 0;
    startup_screen.world_mesh.index_count = 0;
    startup_screen.world_mesh.index_type = VK_INDEX_TYPE_UINT32;

    create_mesh_vbo_final_list(&startup_screen.world_mesh);

    startup_screen.world_render_data.model = matrix4_t(1.0f);
    startup_screen.world_render_data.pbr_info.x = 0.1f;
    startup_screen.world_render_data.pbr_info.y = 0.1f;
    startup_screen.world_render_data.color = vector4_t(0.0f);
}

void w_write_startup_screen(
    world_t *world) {
    vector3_t *temp_vertices = LN_MALLOC(vector3_t, MAX_VERTICES_PER_CHUNK);

    uint32_t vertex_count = 0;  // This is an estimate just to allocate enough memory in serialiser buffer
    for (uint32_t i = 0; i < world->chunks.data_count; ++i) {
        chunk_t *c = world->chunks[i];
        if (c) {
            if (c->render) {
                vertex_count += c->render->mesh.vertex_count;
            }
        }
    }

    serialiser_t serialiser = {};
    // File has spectator position and view direction and visible vertices
    serialiser.init(
        sizeof(vector3_t) * 2 +
        sizeof(uint32_t) +
        vertex_count * sizeof(vector3_t));

    serialiser.serialise_vector3(world->spectator->ws_position);
    serialiser.serialise_vector3(world->spectator->ws_view_direction);

    // Vertex count might change
    uint8_t *vertex_count_ptr = &serialiser.data_buffer[serialiser.data_buffer_head];
    serialiser.serialise_uint32(0);
    vertex_count = 0;

    for (uint32_t i = 0; i < world->chunks.data_count; ++i) {
        chunk_t *c = world->chunks[i];
        if (c) {
            uint32_t current_vertex_count = w_create_chunk_vertices(
                get_surface_level(),
                temp_vertices,
                c,
                world);

            for (uint32_t v = 0; v < current_vertex_count; ++v) {
                serialiser.serialise_vector3(
                    vector3_t(c->render->render_data.model * vector4_t(temp_vertices[v], 1.0f)));
            }

            vertex_count += current_vertex_count;
        }
    }

    serialiser.serialise_uint32(
        vertex_count,
        vertex_count_ptr);

    printf("vertex count: %d\n", vertex_count);

    file_handle_t output = create_file(
        "assets/misc/startup/default.startup",
        (file_load_flags_t)(FLF_BINARY | FLF_WRITEABLE));

    write_file(
        output,
        serialiser.data_buffer,
        serialiser.data_buffer_head);

    free_file(
        output);
}
