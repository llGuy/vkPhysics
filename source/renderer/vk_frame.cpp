#include "vk_imgui.hpp"
#include "vk_buffer.hpp"
#include "vk_context.hpp"
#include "vk_stage_final.hpp"

#include <vk.hpp>

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

void end_frame(const frame_info_t *info) {
    { // Execute final render pass for submission
        VkOffset2D offset;
        memset(&offset, 0, sizeof(offset));

        VkRect2D render_area = {};
        render_area.offset = offset;
        render_area.extent = g_ctx->swapchain.extent;

        VkClearValue clear_value;
        memset(&clear_value, 0, sizeof(VkClearValue));

        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = g_ctx->swapchain.final_pass;
        render_pass_begin_info.framebuffer = g_ctx->swapchain.final_fbos[g_ctx->image_index];
        render_pass_begin_info.renderArea = render_area;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;
    
        vkCmdBeginRenderPass(
            g_ctx->primary_command_buffers[g_ctx->image_index],
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

        g_ctx->pipeline.fin->execute(g_ctx->primary_command_buffers[g_ctx->image_index]);

#if 1
        if (info->debug_window) {
            render_imgui(g_ctx->primary_command_buffers[g_ctx->image_index]);
        }
#endif

        vkCmdEndRenderPass(g_ctx->primary_command_buffers[g_ctx->image_index]);

        end_command_buffer(g_ctx->primary_command_buffers[g_ctx->image_index]);
    }

    { // Submit
        VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &g_ctx->primary_command_buffers[g_ctx->image_index];
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &g_ctx->image_ready_semaphores[g_ctx->current_frame];
        submit_info.pWaitDstStageMask = &wait_stages;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &g_ctx->render_finished_semaphores[g_ctx->current_frame];
 
        VkResult result = vkQueueSubmit(g_ctx->graphics_queue, 1, &submit_info, g_ctx->fences[g_ctx->current_frame]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("FAILED TO SUBMIT WORK TO THE GPU\n");
        }

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &g_ctx->render_finished_semaphores[g_ctx->current_frame];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_ctx->swapchain.swapchain;
        present_info.pImageIndices = &g_ctx->image_index;

        result = vkQueuePresentKHR(g_ctx->present_queue, &present_info);

        if (result != VK_SUCCESS) {
            LOG_ERROR("FAILED TO PRESENT TO SCREEN\n");
        }

        g_ctx->current_frame = (g_ctx->current_frame + 1) % FRAMES_IN_FLIGHT;

        update_sensitive_buffer_deletions();
    
        ++g_ctx->frame_id;
    }
}

}
