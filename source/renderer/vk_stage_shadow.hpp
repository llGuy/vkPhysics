#pragma once

#include "vk_stage_blur.hpp"

namespace vk {

struct shadow_stage_t {
    VkExtent2D map_extent;
    render_pipeline_stage_t stage;
    blur_stage_t blur;

    void init();
};

// These will be public functions, so declared out of struct
void begin_shadow_rendering(VkCommandBuffer cmdbuf);
void end_shadow_rendering(VkCommandBuffer cmdbuf);
 
}
