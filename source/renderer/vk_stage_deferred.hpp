#pragma once

#include "vk_render_pipeline.hpp"

namespace vk {

struct deferred_stage_t {
    render_pipeline_stage_t stage;

    void init_render_pass();
    void init();

    void execute(VkCommandBuffer cmdbuf);
};

}
