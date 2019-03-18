#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

extern "C" {
#   include "../renderer/tr_local.h"
#   include "../renderer/tr_layer.h"
#   include "../qcommon/qcommon.h"
}

#include <vulkan/vulkan.h>

#define VK_PUBLIC extern "C"

void VkCheckError(VkResult result);
#endif
