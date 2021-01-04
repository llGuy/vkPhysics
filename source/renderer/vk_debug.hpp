#pragma once

#include <math.hpp>
#include <vulkan/vulkan.h>

extern PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
extern PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
extern PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
extern PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
extern PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;

namespace vk {

void init_debug_ext_procs();

void set_object_debug_name(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    const char* name);

void set_object_debug_tag(
    uint64_t object,
    VkDebugReportObjectTypeEXT object_type,
    uint64_t name,
    uint32_t tag_size,
    const void *tag);

void begin_debug_region(
    VkCommandBuffer cmdbuf,
    const char *marker_name,
    const vector4_t &color);

void insert_debug(
    VkCommandBuffer cmdbuf,
    const char *name,
    const vector4_t &color);

void end_debug_region(VkCommandBuffer cmdbuf);

}
