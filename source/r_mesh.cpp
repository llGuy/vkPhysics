#include "r_internal.hpp"
#include <vulkan/vulkan.h>
#include "renderer.hpp"

void push_buffer_to_mesh(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    mesh->buffer_type_stack[mesh->buffer_count++] = buffer_type;
    mesh->buffers[buffer_type].type = buffer_type;
}

bool mesh_has_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    return mesh->buffers[buffer_type].type == buffer_type;
}

mesh_buffer_t *get_mesh_buffer(
    buffer_type_t buffer_type,
    mesh_t *mesh) {
    if (mesh_has_buffer(buffer_type, mesh)) {
        return &mesh->buffers[buffer_type];
    }
    else {
        return NULL;
    }
}

void load_mesh_internal(
    internal_mesh_type_t mesh_type,
    mesh_t *mesh) {
    switch (mesh_type) {
    case IM_SPHERE: {
        
    } break;
       
    case IM_CUBE: {
        
    } break;
    }
}
