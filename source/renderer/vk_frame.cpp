#include "vk_frame.hpp"
#include "vk_context.hpp"

namespace vk {

frame_t begin_frame() {
    VkFence null_fence = VK_NULL_HANDLE;

    VkResult result = vkAcquireNextImageKHR(
        g_ctx->device, 
        g_ctx->swapchain.swapchain, 
        UINT64_MAX, 
        g_ctx->image_ready_semaphores[g_ctx->current_frame], 
        null_fence, 
        &g_ctx->image_index);

    vkWaitForFences(g_ctx->device, 1, &g_ctx->fences[g_ctx->current_frame], VK_TRUE, UINT64_MAX);

    vkResetFences(g_ctx->device, 1, &g_ctx->fences[g_ctx->current_frame]);

    begin_command_buffer(g_ctx->primary_command_buffers[g_ctx->image_index], 0, NULL);

    return {
        g_ctx->primary_command_buffers[g_ctx->image_index]
    };
}

}
