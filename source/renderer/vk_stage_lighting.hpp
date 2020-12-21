#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

namespace vk {

struct lighting_stage_t {
    shader_t shader;
    render_pipeline_stage_t stage;

    void init_render_pass();
    void init();
    void execute(VkCommandBuffer cmdbuf);
};

}
