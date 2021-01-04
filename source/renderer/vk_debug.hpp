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

}
