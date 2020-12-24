#include "vk_gpu.hpp"
#include "vk_sync.hpp"
#include "vk_imgui.hpp"
#include "vk_buffer.hpp"
#include "vk_scene3d.hpp"
#include "vk_cmdbuf.hpp"
#include "vk_context.hpp"
#include "vk_present.hpp"
#include "vk_instance.hpp"
#include "app_context.hpp"
#include "vk_descriptor.hpp"

#include <common/allocators.hpp>

namespace vk {

ctx_t *g_ctx = NULL;

void allocate_context() {
    g_ctx = flmalloc<ctx_t>();
}

void deallocate_context() {
    flfree(g_ctx);
    g_ctx = NULL;
}

void init_context() {
    const char *title = "vkPhysics";

    allocate_context();

    app::api_init();
    init_instance(title);
    app::context_info_t app_ctx = app::init_context(title);
    init_surface(&app_ctx);
    init_device();
    init_swapchain(app_ctx.width, app_ctx.height);
    init_command_pool();
    init_primary_command_buffers();
    init_synchronisation();
    init_descriptor_pool();
    init_descriptor_layouts();
    init_imgui(app_ctx.window);
    prepare_scene3d_camera();
    prepare_scene3d_lighting();
    g_ctx->pipeline.allocate();
    g_ctx->pipeline.init();
    prepare_scene3d_environment();
    init_sensitive_buffer_deletions();

    // UI
    g_ctx->frame_id = 0;
}

}
