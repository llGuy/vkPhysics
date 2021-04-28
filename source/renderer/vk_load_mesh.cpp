#include "vk_load_mesh.hpp"

#include "vk.hpp"
#include <files.hpp>
#include <math.hpp>
#include <tools.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>

namespace vk {

vector3_t *get_vertices(
    uint32_t vertices_count,
    serialiser_t *serialiser,
    vector3_t *p) {
    vector3_t *vertices;

    if (p) {
        vertices = p;
    }
    else {
        vertices = lnmalloc<vector3_t>(vertices_count);
    }

    for (uint32_t i = 0; i < vertices_count; ++i) {
        vertices[i] = serialiser->deserialise_vector3();
    }

    return vertices;
}

vector3_t *get_normals(
    uint32_t normals_count,
    serialiser_t *serialiser,
    vector3_t *p) {
    vector3_t *normals;

    if (p) {
        normals = p;
    }
    else {
        normals = lnmalloc<vector3_t>(normals_count);
    }

    for (uint32_t i = 0; i < normals_count; ++i) {
        normals[i] = serialiser->deserialise_vector3();
    }

    return normals;
}

vector2_t *get_tcoords(
    uint32_t tcoords_count,
    serialiser_t *serialiser,
    vector2_t *p) {
    vector2_t *tcoords;

    if (p) {
        tcoords = p;
    }
    else {
        tcoords = lnmalloc<vector2_t>(tcoords_count);
    }

    for (uint32_t i = 0; i < tcoords_count; ++i) {
        tcoords[i].x = serialiser->deserialise_float32();
        tcoords[i].y = serialiser->deserialise_float32();
    }

    return tcoords;
}

vector4_t *get_joint_weights(
    uint32_t joint_weights_count,
    serialiser_t *serialiser,
    vector4_t *p) {
    vector4_t *joint_weights;

    if (p) {
        joint_weights = p;
    }
    else {
        joint_weights = lnmalloc<vector4_t>(joint_weights_count);
    }

    for (uint32_t i = 0; i < joint_weights_count; ++i) {
        joint_weights[i].x = serialiser->deserialise_float32();
        joint_weights[i].y = serialiser->deserialise_float32();
        joint_weights[i].z = serialiser->deserialise_float32();
        joint_weights[i].w = serialiser->deserialise_float32();
        float sum = joint_weights[i].x + joint_weights[i].y + joint_weights[i].z + joint_weights[i].w;
        float q = 1.0f / sum;
        joint_weights[i] *= q;
    }

    return joint_weights;
}

ivector4_t *get_joint_ids(
    uint32_t joint_ids_count,
    serialiser_t *serialiser,
    ivector4_t *p) {
    ivector4_t *joint_ids;

    if (p) {
        joint_ids = p;
    }
    else {
        joint_ids = lnmalloc<ivector4_t>(joint_ids_count);
    }

    for (uint32_t i = 0; i < joint_ids_count; ++i) {
        joint_ids[i].x = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].y = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].z = (int32_t)serialiser->deserialise_uint32();
        joint_ids[i].w = (int32_t)serialiser->deserialise_uint32();
    }

    return joint_ids;
}

uint32_t *get_indices(
    uint32_t indices_count,
    serialiser_t *serialiser) {
    uint32_t *indices = lnmalloc<uint32_t>(indices_count);

    for (uint32_t i = 0; i < indices_count; ++i) {
        indices[i] = serialiser->deserialise_uint32();
    }

    return indices;
}

void mesh_loader_t::push_attrib(buffer_type_t buffer_type, void *p, uint32_t size) {
    buffer_type_stack[attrib_count++] = buffer_type;
    attribs[buffer_type].type = buffer_type;
    attribs[buffer_type].data = p;
    attribs[buffer_type].size = size;
}

bool mesh_loader_t::has_attrib(buffer_type_t buffer_type) const {
    return attribs[buffer_type].type == buffer_type;
}

mesh_attrib_t *mesh_loader_t::get_attrib(buffer_type_t buffer_type) {
    if (has_attrib(buffer_type)) {
        return &attribs[buffer_type];
    }
    else {
        return NULL;
    }
}

const mesh_attrib_t *mesh_loader_t::get_attrib(buffer_type_t buffer_type) const {
    if (has_attrib(buffer_type)) {
        return &attribs[buffer_type];
    }
    else {
        return NULL;
    }
}

void mesh_loader_t::load_sphere() {
    int32_t sector_count = 16;
    int32_t stack_count = 16;
    
    vertex_count = (sector_count + 1) * (stack_count + 1);
    index_count = sector_count * stack_count * 6;
    
    vector3_t *positions = lnmalloc<vector3_t>(vertex_count);
    uint32_t *indices = lnmalloc<uint32_t>(index_count);
    
    float sector_step = 2.0f * PI / sector_count;
    float stack_step = PI / stack_count;
    float sector_angle = 0;
    float stack_angle = 0;

    uint32_t counter = 0;
    
    for (int32_t i = 0; i <= stack_count; ++i) {
        stack_angle = PI / 2.0f - i * stack_step;
        float xy = cos(stack_angle);
        float z = sin(stack_angle);

        for (int32_t j = 0; j <= sector_count; ++j) {
            sector_angle = j * sector_step;

            float x = xy * cos(sector_angle);
            float y = xy * sin(sector_angle);

            positions[counter++] = vector3_t(x, z, y);
        }
    }

    counter = 0;
    
    for (int32_t i = 0; i < stack_count; ++i) {
        int k1 = i * (sector_count + 1);
        int k2 = k1 + sector_count + 1;

        for (int32_t j = 0; j < sector_count; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices[counter++] = k1;
                indices[counter++] = k2;
                indices[counter++] = k1 + 1;
            }

            if (i != (stack_count - 1)) {
                indices[counter++] = k1 + 1;
                indices[counter++] = k2;
                indices[counter++] = k2 + 1;
            }
        }
    }

    index_count = counter;

    push_attrib(BT_INDICES, indices, sizeof(uint32_t) * index_count);
    push_attrib(BT_VERTEX, positions, sizeof(vector3_t) * vertex_count);

    vertex_offset = 0;
    first_index = 0;
    index_offset = 0;
    index_type = VK_INDEX_TYPE_UINT32;
}

void mesh_loader_t::load_cube() {
    vector3_t *vertices = lnmalloc<vector3_t>(8);

    vertices[0] = vector3_t(-1.0f, -1.0f, 1.0f);
    vertices[1] = vector3_t(1.0f, -1.0f, 1.0f);
    vertices[2] = vector3_t(1.0f, 1.0f, 1.0f);
    vertices[3] = vector3_t(-1.0f, 1.0f, 1.0f);
    vertices[4] = vector3_t(-1.0f, -1.0f, -1.0f);
    vertices[5] = vector3_t(1.0f, -1.0f, -1.0f);
    vertices[6] = vector3_t(1.0f, 1.0f, -1.0f);
    vertices[7] = vector3_t(-1.0f, 1.0f, -1.0f);

    uint32_t *indices = lnmalloc<uint32_t>(36);

    uint32_t indices_st[36] = {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        7, 6, 5, 5, 4, 7,
        3, 7, 4, 4, 0, 3,
        4, 5, 1, 1, 0, 4,
        3, 2, 6, 6, 7, 3
    };

    memcpy(indices, indices_st, sizeof(uint32_t) * 36);

    index_count = sizeof(indices) / sizeof(indices[0]);
    vertex_count = sizeof(vertices) / sizeof(vertices[0]);

    push_attrib(BT_INDICES, indices, sizeof(uint32_t) * index_count);
    push_attrib(BT_VERTEX, vertices, sizeof(vector3_t) * vertex_count);

    vertex_offset = 0;
    first_index = 0;
    index_offset = 0;
    index_type = VK_INDEX_TYPE_UINT32;
}

void mesh_loader_t::load_external(const char *path) {
    file_handle_t file_handle = create_file(path, FLF_BINARY);
    file_contents_t contents = read_file(file_handle);
    free_file(file_handle);
    
    serialiser_t serialiser = {};
    serialiser.data_buffer = (uint8_t *)contents.data;
    serialiser.data_buffer_size = contents.size;

    header_t file_header = {};
    file_header.vertices_count = serialiser.deserialise_uint32();
    file_header.normals_count = serialiser.deserialise_uint32();
    file_header.tcoords_count = serialiser.deserialise_uint32();
    file_header.joint_weights_count = serialiser.deserialise_uint32();
    file_header.joint_ids_count = serialiser.deserialise_uint32();
    file_header.indices_count = serialiser.deserialise_uint32();

    vector3_t *vertices = get_vertices(file_header.vertices_count, &serialiser);
    vector3_t *normals = get_normals(file_header.normals_count, &serialiser);
    vector2_t *tcoords = get_tcoords(file_header.tcoords_count, &serialiser);
    vector4_t *joint_weights = get_joint_weights(file_header.joint_weights_count, &serialiser);
    ivector4_t *joint_ids = get_joint_ids(file_header.joint_ids_count, &serialiser);
    uint32_t *indices = get_indices(file_header.indices_count, &serialiser);

    push_attrib(BT_INDICES, indices, sizeof(uint32_t) * file_header.indices_count);
    push_attrib(BT_VERTEX, vertices, sizeof(vector3_t) * file_header.vertices_count);
    push_attrib(BT_NORMAL, normals, sizeof(vector3_t) * file_header.normals_count);
    if (file_header.joint_weights_count)
        push_attrib(BT_JOINT_WEIGHT, joint_weights, sizeof(vector4_t) * file_header.joint_weights_count);
    if (file_header.joint_ids_count)
        push_attrib(BT_JOINT_INDICES, joint_ids, sizeof(ivector4_t) * file_header.joint_ids_count);

    vertex_offset = 0;
    vertex_count = file_header.vertices_count;
    first_index = 0;
    index_offset = 0;
    index_count = file_header.indices_count;
    index_type = VK_INDEX_TYPE_UINT32;
}

}
