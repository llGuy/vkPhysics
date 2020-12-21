#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

namespace vk {

struct ui_stage_t {
    render_pipeline_stage_t stage;
    // This just renders the previous stage
    shader_t shader;
    VkCommandBuffer render_previous_cmdbufs[3];
    uint32_t render_previous_index;

    void init_render_pass();
    void init();

    void execute(VkCommandBuffer cmdbuf, VkCommandBuffer ui_cmdbuf);
};

}
