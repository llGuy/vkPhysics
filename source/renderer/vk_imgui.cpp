#include "vk_imgui.hpp"
#include "vk_context.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace vk {

static void s_imgui_callback(VkResult result) {
    // Do something maybe
}

void init_imgui(void *win) {
    g_ctx->debug.ui_proc_count = 0;

    GLFWwindow *window = (GLFWwindow *)win;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    VkAttachmentDescription attachment = {};
    attachment.format = g_ctx->swapchain.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(g_ctx->device, &info, nullptr, &g_ctx->debug.imgui_render_pass));

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_ctx->instance;
    init_info.PhysicalDevice = g_ctx->hardware;
    init_info.Device = g_ctx->device;
    init_info.QueueFamily = g_ctx->queue_families.graphics_family;
    init_info.Queue = g_ctx->graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = g_ctx->descriptor_pool;
    init_info.Allocator = NULL;
    init_info.MinImageCount = g_ctx->swapchain.image_count;
    init_info.ImageCount = g_ctx->swapchain.image_count;
    init_info.CheckVkResultFn = &s_imgui_callback;
    ImGui_ImplVulkan_Init(&init_info, g_ctx->debug.imgui_render_pass);

    ImGui::StyleColorsDark();

    VkCommandBuffer command_buffer = begin_single_use_command_buffer();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_single_use_command_buffer(command_buffer);
}

void add_debug_ui_proc(debug_ui_proc_t proc) {
    g_ctx->debug.ui_procs[g_ctx->debug.ui_proc_count++] = proc;
}

void render_imgui(VkCommandBuffer cmdbuf) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // General stuff
    ImGui::Begin("General");
    ImGui::Text("Framerate: %.1f", ImGui::GetIO().Framerate);

    for (uint32_t i = 0; i < g_ctx->debug.ui_proc_count; ++i) {
        (g_ctx->debug.ui_procs[i])();
    }

    ImGui::End();

    ImGui::Render();

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), g_ctx->primary_command_buffers[g_ctx->image_index]);
}

}
