#pragma once

#include "vk_shader.hpp"
#include "vk_render_pipeline.hpp"

namespace vk {

struct final_stage_t {
    render_pipeline_stage_t stage;
    shader_t shader;

    float main_screen_brightness;

    void init();
    void execute(VkCommandBuffer cmdbuf);
};

}
