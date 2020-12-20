#pragma once

#include <vulkan/vulkan.hpp>

namespace vk {

constexpr uint32_t MAX_DEBUG_UI_PROCS = 10;

typedef void (*debug_ui_proc_t)();

struct imgui_data_t {
    VkRenderPass imgui_render_pass;
    uint32_t ui_proc_count;
    debug_ui_proc_t ui_procs[MAX_DEBUG_UI_PROCS];
};

void init_imgui(void *window);
void add_debug_ui_proc(debug_ui_proc_t proc);

}
