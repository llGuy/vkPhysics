#pragma once

#include "vk_shader.hpp"
#include "vk_buffer.hpp"

#include <vk.hpp>
#include <stdint.h>

namespace vk {

void create_player_merged_mesh(
    mesh_t *dst_a, shader_binding_info_t *sbi_a,
    mesh_t *dst_b, shader_binding_info_t *sbi_b,
    mesh_t *dst_merged, shader_binding_info_t *sbi_merged);

// By default, a uniform buffer is added for camera transforms
// If STATIC is chosen, no extra descriptors will be added to shader information
// If ANIMATED is chosen, 1 extra uniform buffer will be added for joint transforms
// If PASS_EXTRA_UNIFORM_BUFFER, all uniforms will be added from previous options, and
// Extra ones will be added that you pass in the extra descriptors array
enum mesh_type_t {
    MT_STATIC = 1 << 0,
    MT_ANIMATED = 1 << 1,
    MT_PASS_EXTRA_UNIFORM_BUFFER = 1 << 2,
    MT_MERGED_MESH = 1 << 3
};

typedef int32_t mesh_type_flags_t;

shader_t create_mesh_shader_color(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkCullModeFlags culling,
    VkPrimitiveTopology topology,
    mesh_type_flags_t type);

shader_t create_mesh_shader_shadow(
    shader_binding_info_t *binding_info,
    const char **shader_paths,
    VkShaderStageFlags shader_flags,
    VkPrimitiveTopology topology,
    mesh_type_flags_t mesh_type);

}
