#include "t_types.hpp"
#include "tools.hpp"
#include "vk.hpp"
#include "vk_buffer.hpp"
#include "vk_scene3d.hpp"
#include "vk_context.hpp"
#include "vk_load_mesh.hpp"
#include "vk_animation.hpp"
#include "vk_stage_shadow.hpp"
#include "vk_render_pipeline.hpp"

#include <cassert>
#include <cstring>
#include <log.hpp>
#include <vulkan/vulkan.h>
#include <math.hpp>
#include <files.hpp>
#include <serialiser.hpp>
#include <allocators.hpp>
#include <vulkan/vulkan_core.h>

namespace vk {

void mesh_t::push_buffer(buffer_type_t buffer_type) {
    buffer_type_stack[buffer_count++] = buffer_type;
    buffers[buffer_type].type = buffer_type;
}

bool mesh_t::has_buffer(buffer_type_t buffer_type) {
    return buffers[buffer_type].type == buffer_type;
}

mesh_buffer_t *mesh_t::get_mesh_buffer(buffer_type_t buffer_type) {
    if (has_buffer(buffer_type)) {
        return &buffers[buffer_type];
    }
    else {
        return NULL;
    }
}

shader_binding_info_t mesh_t::create_shader_binding_info() {
    shader_binding_info_t info = {};
    uint32_t start_index = 0;
    if (has_buffer(BT_INDICES)) {
        start_index = 1;
    }

    uint32_t binding_count = buffer_count - start_index;

    info.binding_count = binding_count;
    info.binding_descriptions = FL_MALLOC(VkVertexInputBindingDescription, info.binding_count);

    // Each binding will store one attribute
    info.attribute_count = binding_count;
    info.attribute_descriptions = FL_MALLOC(VkVertexInputAttributeDescription, info.binding_count);

    for (uint32_t i = 0; start_index < buffer_count; ++start_index, ++i) {
        VkVertexInputBindingDescription *current_binding = &info.binding_descriptions[i];
        VkVertexInputAttributeDescription *current_attribute = &info.attribute_descriptions[i];

        // These depend on the buffer type
        VkFormat format;
        uint32_t stride;

        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(buffer_type_stack[start_index]);
        
        switch (current_mesh_buffer->type) {
        case BT_VERTEX: case BT_NORMAL: {
            format = VK_FORMAT_R32G32B32_SFLOAT;
            stride = sizeof(vector3_t);
        } break;
        case BT_UVS: {
            format = VK_FORMAT_R32G32_SFLOAT;
            stride = sizeof(vector2_t);
        } break;
        case BT_JOINT_WEIGHT: {
            format = VK_FORMAT_R32G32B32A32_SFLOAT;
            stride = sizeof(vector4_t);
        } break;
        case BT_JOINT_INDICES: {
            format = VK_FORMAT_R32G32B32A32_SINT;
            stride = sizeof(ivector4_t);
        } break;
        default: {
            LOG_ERRORV("Mesh buffer %d not supported\n", current_mesh_buffer->type);
            exit(1);
        } break;
        }
        
        current_binding->binding = i;
        current_binding->stride = stride;
        current_binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        current_attribute->binding = i;
        current_attribute->location = i;
        current_attribute->format = format;
        current_attribute->offset = 0;
    }

    return info;
}

void mesh_t::init_mesh_vbo_final_list() {
    uint32_t vb_count = has_buffer(BT_INDICES) ? buffer_count - 1 : buffer_count;
    vertex_buffer_count = vb_count;

    uint32_t counter = 0;
    for (uint32_t i = 0; i < buffer_count; ++i) {
        mesh_buffer_t *current_mesh_buffer = get_mesh_buffer(buffer_type_stack[i]);
        
        if (current_mesh_buffer->type == BT_INDICES) {
            index_buffer = current_mesh_buffer->gpu_buffer.buffer;
        }
        else {
            vertex_buffers_final[counter++] = current_mesh_buffer->gpu_buffer.buffer;
        }
    }

    for (uint32_t i = 0; i < vb_count; ++i) {
        vertex_buffers_offsets[i] = vertex_offset;
    }
}

void mesh_t::load(mesh_loader_t *loader, shader_binding_info_t *dst_bindings) {
    this->vertex_offset = loader->vertex_offset;
    this->vertex_count = loader->vertex_count;
    this->first_index = loader->first_index;
    this->index_count = loader->index_count;
    this->index_offset = loader->index_offset;
    this->index_type = loader->index_type;

    for (uint32_t i = 0; i < loader->attrib_count; ++i) {
        buffer_type_t bt = loader->buffer_type_stack[i];
        mesh_attrib_t *attrib = loader->get_attrib(bt);

        this->push_buffer(bt);
        mesh_buffer_t *mesh_buffer = this->get_mesh_buffer(bt);

        switch (bt) {
        case BT_INDICES: {
            mesh_buffer->gpu_buffer.init(
                attrib->size,
                attrib->data,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        } break;

        default: {
            mesh_buffer->gpu_buffer.init(
                attrib->size,
                attrib->data,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        } break;
        }
    }

    if (dst_bindings) {
        *dst_bindings = create_shader_binding_info();
    }

    init_mesh_vbo_final_list();
}

static uint32_t s_allocate_memory_for_merged_mesh_attrib(buffer_type_t type, buffer_t *b, uint32_t count) {
    switch (type) {
    case BT_JOINT_INDICES: {
        b->p = lnmalloc<ivector4_t>(count);
        b->size = sizeof(ivector4_t) * count;

        return sizeof(ivector4_t);
    } break;
                              
    case BT_JOINT_WEIGHT: {
        b->p = lnmalloc<vector4_t>(count);
        b->size = sizeof(vector4_t) * count;

        return sizeof(vector4_t);
    } break;

    case BT_UVS: {
        b->p = lnmalloc<vector2_t>(count);
        b->size = sizeof(vector2_t) * count;

        return sizeof(vector2_t);
    } break;

    case BT_COLOR: {
        b->p = lnmalloc<vector4_t>(count);
        b->size = sizeof(vector4_t) * count;

        return sizeof(vector4_t);
    } break;

    default: {
        b->p = lnmalloc<vector3_t>(count);
        b->size = sizeof(vector3_t) * count;

        return sizeof(vector3_t);
    } break;
    }
}

static VkBufferUsageFlagBits s_get_buffer_usage(buffer_type_t type) {
    switch (type) {
    case BT_INDICES: {
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    } break;

    default: {
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    } break;
    }
}

void create_merged_mesh(merged_mesh_info_t *info) {
    const mesh_loader_t *largest = info->sk_loader;
    const mesh_loader_t *smallest = info->st_loader;

    if (largest->attrib_count < smallest->attrib_count) {
        const mesh_loader_t *actual_largest = smallest;
        smallest = largest;
        largest = actual_largest;
    }

    buffer_t buffers[MAX_MESH_BUFFERS] = {};
    buffer_type_t total_bt[BT_MAX_VALUE] = {};
    uint32_t total_buffer_count = 0;

    uint32_t largest_vtx_count = largest->vertex_count;
    uint32_t smallest_vtx_count = smallest->vertex_count;

    for (uint32_t i = 1; i < BT_MAX_VALUE; ++i) {
        bool largest_has = largest->has_attrib((buffer_type_t)i);
        bool smallest_has = smallest->has_attrib((buffer_type_t)i);

        if (largest_has || smallest_has) {
            total_bt[total_buffer_count++] = (buffer_type_t)i;

            if ((buffer_type_t)i == BT_INDICES) {
                /*
                  Create indices such that every triangle alternates between largest and smallest mesh.
                */
                const mesh_attrib_t *l_attrib = largest->get_attrib((buffer_type_t)i);
                const mesh_attrib_t *s_attrib = smallest->get_attrib((buffer_type_t)i);

                uint32_t *l_indices = (uint32_t *)l_attrib->data;
                uint32_t *s_indices = (uint32_t *)s_attrib->data;

                uint32_t indices_count = MAX(smallest->index_count, largest->index_count) * 2;
                uint32_t *indices = lnmalloc<uint32_t>(indices_count);

                for (uint32_t i = 0; i < indices_count; i += 6) {
                    uint32_t largest_src_idx_start = i / 2;
                    uint32_t smallest_src_idx_start = i / 2;

                    if (smallest_src_idx_start >= smallest->index_count) {
                        indices[i + 3] = l_indices[0];
                        indices[i + 4] = l_indices[1];
                        indices[i + 5] = l_indices[2];
                    }
                    else {
                        indices[i + 3] = s_indices[smallest_src_idx_start] + largest_vtx_count;
                        indices[i + 4] = s_indices[smallest_src_idx_start + 1] + largest_vtx_count;
                        indices[i + 5] = s_indices[smallest_src_idx_start + 2] + largest_vtx_count;
                    }

                    if (largest_src_idx_start >= largest->index_count) {
                        indices[i] = s_indices[0];
                        indices[i + 1] = s_indices[1];
                        indices[i + 2] = s_indices[2];
                    }
                    else {
                        indices[i] = l_indices[largest_src_idx_start];
                        indices[i + 1] = l_indices[largest_src_idx_start + 1];
                        indices[i + 2] = l_indices[largest_src_idx_start + 2];
                    }
                }

                buffer_t *b = &buffers[i];
                b->p = indices;
                b->size = indices_count * sizeof(uint32_t);

                info->dst_merged->index_count = indices_count;
            }
            else {
                /*
                  For all other attributes, just create a buffer and memcpy the attribute data
                  of the largest mesh, then the attribute data of the smallest mesh.
                */
                buffer_t *b = &buffers[i];
                uint32_t sizeof_element = s_allocate_memory_for_merged_mesh_attrib(
                    (buffer_type_t)i,
                    b,
                    largest_vtx_count + smallest_vtx_count);

                // First put the largest data if it exists, then the smallest data if it exists
                if (largest_has) {
                    const mesh_attrib_t *attrib = largest->get_attrib((buffer_type_t)i);

                    assert(attrib->size == largest_vtx_count * sizeof_element);
                    memcpy(b->p, attrib->data, attrib->size);
                }
                else {
                    memset(b->p, 0, largest_vtx_count * sizeof_element);
                }

                if (smallest_has) {
                    const mesh_attrib_t *attrib = smallest->get_attrib((buffer_type_t)i);

                    assert(attrib->size == smallest_vtx_count * sizeof_element);
                    memcpy((uint8_t *)b->p + sizeof_element * largest_vtx_count, attrib->data, attrib->size);
                }
                else {
                    memset((uint8_t *)b->p + sizeof_element * largest_vtx_count, 0, smallest_vtx_count * sizeof_element);
                }
            }

            // Push the buffer to the merged mesh
            buffer_t *b = &buffers[i];
            info->dst_merged->push_buffer((buffer_type_t)i);
            mesh_buffer_t *mesh_buf = info->dst_merged->get_mesh_buffer((buffer_type_t)i);
            mesh_buf->gpu_buffer.init(b->size, b->p, s_get_buffer_usage((buffer_type_t)i));
        }
    }

    info->dst_merged->vertex_offset = 0;
    info->dst_merged->vertex_count = largest_vtx_count + smallest_vtx_count;
    info->dst_merged->first_index = 0;
    info->dst_merged->index_offset = 0;
    // Is already set
    // info->dst_merged->index_count = indices_count;
    info->dst_merged->index_type = VK_INDEX_TYPE_UINT32;
    *(info->dst_merged_sbi) = info->dst_merged->create_shader_binding_info();
    info->dst_merged->init_mesh_vbo_final_list();

    // Create mesh for skeletal mesh
    for (uint32_t i = 0; i < info->sk_loader->attrib_count; ++i) {
        buffer_type_t t = info->sk_loader->buffer_type_stack[i];
        const mesh_attrib_t *attrib = &info->sk_loader->attribs[(uint32_t)t];

        info->dst_sk->push_buffer(t);
        mesh_buffer_t *mesh_buf = info->dst_sk->get_mesh_buffer(t);

        if (t == BT_INDICES) {
            mesh_buf->gpu_buffer.init(attrib->size, attrib->data, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }
        else {
            mesh_buffer_t *merged_mesh_buf = info->dst_merged->get_mesh_buffer(t);
            mesh_buf->gpu_buffer = merged_mesh_buf->gpu_buffer;
        }
    }

    info->dst_sk->vertex_offset = 0;
    info->dst_sk->vertex_count = info->sk_loader->vertex_count;
    info->dst_sk->first_index = 0;
    info->dst_sk->index_offset = 0;
    info->dst_sk->index_count = info->sk_loader->index_count;
    info->dst_sk->index_type = VK_INDEX_TYPE_UINT32;
    *(info->dst_sk_sbi) = info->dst_sk->create_shader_binding_info();
    info->dst_sk->init_mesh_vbo_final_list();

    // Create mesh for static mesh
    for (uint32_t i = 0; i < info->st_loader->attrib_count; ++i) {
        buffer_type_t t = info->st_loader->buffer_type_stack[i];
        const mesh_attrib_t *attrib = &info->st_loader->attribs[(uint32_t)t];

        info->dst_st->push_buffer(t);
        mesh_buffer_t *mesh_buf = info->dst_st->get_mesh_buffer(t);

        if (t == BT_INDICES) {
            mesh_buf->gpu_buffer.init(attrib->size, attrib->data, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }
        else {
            mesh_buffer_t *merged_mesh_buf = info->dst_merged->get_mesh_buffer(t);
            mesh_buf->gpu_buffer = merged_mesh_buf->gpu_buffer;
        }
    }

    info->dst_st->vertex_offset = sizeof(vector3_t) * info->sk_loader->vertex_count;
    info->dst_st->vertex_count = info->st_loader->vertex_count;
    info->dst_st->first_index = 0;
    info->dst_st->index_offset = 0;
    info->dst_st->index_count = info->st_loader->index_count;
    info->dst_st->index_type = VK_INDEX_TYPE_UINT32;
    *(info->dst_st_sbi) = info->dst_st->create_shader_binding_info();
    info->dst_st->init_mesh_vbo_final_list();
}

shader_t create_mesh_shader_color(shader_create_info_t *info, mesh_type_flags_t mesh_type) {
    VkDescriptorType types[] = {
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    uint32_t type_count = 1;
    if (mesh_type & MT_ANIMATED) {
        type_count++;
    }

    if (mesh_type & MT_PASS_EXTRA_UNIFORM_BUFFER) {
        type_count++;
    }

    uint32_t push_constant_size = sizeof(mesh_render_data_t);
    if (mesh_type & MT_MERGED_MESH) {
        push_constant_size += sizeof(matrix4_t) + sizeof(float);
    }

    info->descriptor_layout_types = types;
    info->descriptor_layout_count = type_count;
    info->push_constant_size = push_constant_size;

    shader_t s = {};
    s.init_as_3d_shader_for_stage(info, ST_DEFERRED);

    return s;
}

// shader_t create_mesh_shader_shadow(
//     shader_binding_info_t *binding_info,
//     const char **shader_paths,
//     VkShaderStageFlags shader_flags,
//     VkPrimitiveTopology topology,
//     mesh_type_flags_t mesh_type) {
shader_t create_mesh_shader_shadow(shader_create_info_t *info, mesh_type_flags_t mesh_type) {
    if (mesh_type == MT_STATIC) {
        VkDescriptorType ubo_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        uint32_t push_constant_size = sizeof(mesh_render_data_t);
        if (mesh_type & MT_MERGED_MESH) {
            push_constant_size += sizeof(matrix4_t) + sizeof(float);
        }

        info->push_constant_size = push_constant_size;
        info->descriptor_layout_count = 1;
        info->descriptor_layout_types = &ubo_type;
        info->cull_mode = VK_CULL_MODE_FRONT_BIT;

        shader_t s = {}; s.init_as_3d_shader_for_stage(info, ST_SHADOW);
    
        return s;
    }
    else {
        VkDescriptorType ubo_types[] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};

        uint32_t push_constant_size = sizeof(mesh_render_data_t);
        if (mesh_type & MT_MERGED_MESH) {
            push_constant_size += sizeof(matrix4_t) + sizeof(float);
        }

        info->push_constant_size = push_constant_size;
        info->descriptor_layout_count = 2;
        info->descriptor_layout_types = ubo_types;
        info->cull_mode = VK_CULL_MODE_FRONT_BIT;

        shader_t s = {};
        s.init_as_3d_shader_for_stage(info, ST_SHADOW);

        return s;
    }
}

void submit_mesh(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data) {
    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, (uint32_t)render_data.size, render_data.p);
    
    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_mesh_shadow(VkCommandBuffer cmdbuf, mesh_t *mesh, shader_t *shader, const buffer_t &render_data) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->pipeline.shadow->map_extent.width;
    viewport.height = (float)g_ctx->pipeline.shadow->map_extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->pipeline.shadow->map_extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();
    VkDescriptorSet lighting_transforms = scene_sets.lighting_ubo;

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        1,
        &lighting_transforms,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, (uint32_t)render_data.size, render_data.p);

    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet camera_transforms = scene_sets.camera_ubo;
    VkDescriptorSet descriptor_sets[] = {camera_transforms, instance->descriptor_set};

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, (uint32_t)render_data.size, render_data.p);
    
    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, 0, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void submit_skeletal_mesh_shadow(
    VkCommandBuffer cmdbuf,
    mesh_t *mesh,
    shader_t *shader,
    const buffer_t &render_data,
    animated_instance_t *instance) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->pipeline.shadow->map_extent.width;
    viewport.height = (float)g_ctx->pipeline.shadow->map_extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->pipeline.shadow->map_extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet lighting_transforms = scene_sets.lighting_ubo;
    VkDescriptorSet descriptor_sets[] = { lighting_transforms, instance->descriptor_set };

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        2,
        descriptor_sets,
        0,
        NULL);

    vkCmdPushConstants(cmdbuf, shader->layout, shader->flags, 0, (uint32_t)render_data.size, render_data.p);

    if (mesh->has_buffer(BT_INDICES)) {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdBindIndexBuffer(cmdbuf, mesh->index_buffer, mesh->index_offset, mesh->index_type);

        vkCmdDrawIndexed(cmdbuf, mesh->index_count, 1, mesh->first_index, mesh->vertex_offset, 0);
    }
    else {
        vkCmdBindVertexBuffers(cmdbuf, 0, mesh->vertex_buffer_count, mesh->vertex_buffers_final, mesh->vertex_buffers_offsets);
        vkCmdDraw(cmdbuf, mesh->vertex_count, 1, mesh->vertex_offset, 0);
    }
}

void begin_mesh_submission(
    VkCommandBuffer cmdbuf,
    shader_t *shader,
    VkDescriptorSet extra_set) {
    VkViewport viewport = {};
    viewport.width = (float)g_ctx->swapchain.extent.width;
    viewport.height = (float)g_ctx->swapchain.extent.height;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D rect = {};
    rect.extent = g_ctx->swapchain.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &rect);
    
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

    scene3d_descriptors_t scene_sets = get_scene_descriptors();

    VkDescriptorSet sets[] = {scene_sets.camera_ubo, extra_set};
    uint32_t count = (extra_set == VK_NULL_HANDLE) ? 1 : 2;

    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->layout,
        0,
        count,
        sets,
        0,
        NULL);
}

}
