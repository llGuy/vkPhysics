#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

namespace vk {

struct post_stage_t {
    shader_t shader;
    render_pipeline_stage_t stage;
    uint32_t post_process_cmdbuf_count;
    VkCommandBuffer post_process_cmdbufs[3];

    void init_render_pass();
    void init();

    void execute(VkCommandBuffer cmdbuf, VkCommandBuffer ui_cmdbuf);
};

}
