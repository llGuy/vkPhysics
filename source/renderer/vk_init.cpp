#include "vk_init.hpp"
#include "renderer/vk_buffer.hpp"
#include "renderer/vk_descriptor.hpp"
#include "renderer/vk_imgui.hpp"
#include "renderer/vk_scene3d.hpp"
#include "renderer/vk_sync.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_gpu.hpp"
#include "vk_present.hpp"
#include "vk_instance.hpp"
#include "app_context.hpp"

namespace vk {

void big_init() {
    app::api_init();
    init_instance("vkPhysics");
    app::context_info_t app_ctx = app::init_context();
    init_surface(&app_ctx);
    init_device();
    init_swapchain(app_ctx.width, app_ctx.height);
    init_command_pool();
    init_primary_command_buffers();
    init_synchronisation();
    init_descriptor_pool();
    init_descriptor_layouts();
    init_imgui(app_ctx.window);
    prepare_scene3d_data();
    g_ctx->pipeline.init();
    init_sensitive_buffer_deletions();

    // UI
    g_ctx->frame_id = 0;
}

}
