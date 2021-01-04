#include "vk_debug.hpp"
#include "vk_context.hpp"

PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;

namespace vk {

static bool is_debug_proc_available = 0;

void init_debug_ext_procs() {
    vkDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(g_ctx->device, "vkDebugMarkerSetObjectTagEXT");
    vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(g_ctx->device, "vkDebugMarkerSetObjectNameEXT");
    vkCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(g_ctx->device, "vkCmdDebugMarkerBeginEXT");
    vkCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(g_ctx->device, "vkCmdDebugMarkerEndEXT");
    vkCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(g_ctx->device, "vkCmdDebugMarkerInsertEXT");

    if (vkDebugMarkerSetObjectTag == VK_NULL_HANDLE) {
        LOG_ERROR("Vulkan debug marker functions were not found\n");
        is_debug_proc_available = 0;
    }
    else {
        LOG_INFO("Vulkan debug marker functions were found\n");
        is_debug_proc_available = 1;
    }
}

void set_object_debug_name(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    const char *name) {
    if (is_debug_proc_available) {
        VkDebugMarkerObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        name_info.objectType = object_type;
        name_info.object = object;
        name_info.pObjectName = name;
        vkDebugMarkerSetObjectName(g_ctx->device, &name_info);
    }
}

void set_object_debug_tag(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    uint64_t name,
    uint32_t tag_size,
    const void *tag) {
    if (is_debug_proc_available) {
        VkDebugMarkerObjectTagInfoEXT tag_info = {};
        tag_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
        tag_info.objectType = object_type;
        tag_info.object = object;
        tag_info.tagName = name;
        tag_info.tagSize = tag_size;
        tag_info.pTag = tag;
        vkDebugMarkerSetObjectTag(g_ctx->device, &tag_info);
    }
}

void begin_debug_region(VkCommandBuffer cmdbuf, const char *marker_name, const vector4_t &color) {
    if (is_debug_proc_available) {
        VkDebugMarkerMarkerInfoEXT marker_info = {};
        marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        memcpy(marker_info.color, &color[0], sizeof(float) * 4);
        marker_info.pMarkerName = marker_name;
        vkCmdDebugMarkerBegin(cmdbuf, &marker_info);
    }
}

void insert_debug(VkCommandBuffer cmdbuf, const char *name, const vector4_t &color) {
    if (is_debug_proc_available) {
        VkDebugMarkerMarkerInfoEXT marker_info = {};
        marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        memcpy(marker_info.color, &color[0], sizeof(float) * 4);
        marker_info.pMarkerName = name;
        vkCmdDebugMarkerInsert(cmdbuf, &marker_info);
    }
}

void end_debug_region(VkCommandBuffer cmdbuf) {
    if (is_debug_proc_available && vkCmdDebugMarkerEnd) {
        vkCmdDebugMarkerEnd(cmdbuf);
    }
}

}
