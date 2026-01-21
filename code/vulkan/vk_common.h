#pragma once

// Standard C++ headers (must come first to avoid conflicts)
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdint>

// Vulkan platform definition (must be before vulkan.h)
#define VK_USE_PLATFORM_WIN32_KHR

// Vulkan headers
#include <vulkan/vulkan.h>



// Engine headers (C headers wrapped in extern "C")
#ifdef __cplusplus
extern "C" {
#endif
#include "../renderer/tr_local.h"
#include "../renderer/tr_layer.h"
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------------
// Error checking macro
//----------------------------------------------------------------------------
#define VK_CHECK(x) { \
    VkResult res = (x); \
    if (res != VK_SUCCESS) { \
        ri.Error(ERR_FATAL, "Vulkan Error 0x%08X at %s:%d: %s\n", res, __FILE__, __LINE__, #x); \
    } \
}

#define VK_CHECK_ASSIGN(var, x) { \
    VkResult res = (x); \
    if (res != VK_SUCCESS) { \
        ri.Error(ERR_FATAL, "Vulkan Error 0x%08X at %s:%d: %s\n", res, __FILE__, __LINE__, #x); \
    } \
    var = res; \
}

//----------------------------------------------------------------------------
// Safe release macro
//----------------------------------------------------------------------------
#define SAFE_DESTROY(obj, destroyFunc) { \
    if (obj) { \
        destroyFunc; \
        obj = VK_NULL_HANDLE; \
    } \
}

//----------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------
#define VK_MAX_FRAMES_IN_FLIGHT 2
#define VK_CIRCULAR_BUFFER_SIZE (4 * 1024 * 1024)  // 4MB default
#define VK_INDEX_BUFFER_SIZE (2 * 1024 * 1024)     // 2MB
#define VK_VERTEX_BUFFER_SIZE (4 * 1024 * 1024)    // 4MB

//----------------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------------
struct vkImage_t;
struct vkCircularBuffer_t;
struct vkFrameData_t;

//----------------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------------
namespace VKUtil {
    // Get Vulkan format from engine format
    VkFormat GetVkFormat(imageFormat_t fmt);

    // Get blend factor from GLS_ flags
    VkBlendFactor GetSrcBlendFactor(unsigned long stateMask);
    VkBlendFactor GetDstBlendFactor(unsigned long stateMask);

    // Get compare op from GLS_ flags
    VkCompareOp GetDepthCompareOp(unsigned long stateMask);

    // Get cull mode from CT_ flags
    VkCullModeFlags GetCullMode(int cullType);

    // Get polygon mode from GLS_ flags
    VkPolygonMode GetPolygonMode(unsigned long stateMask);

    // Find memory type index
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Create shader module from SPIR-V file
    VkShaderModule LoadShaderModule(VkDevice device, const char* filename);
}

//----------------------------------------------------------------------------
// Debug logging
//----------------------------------------------------------------------------
#ifdef _DEBUG
#   define VK_LOG(...) ri.Printf(PRINT_ALL, __VA_ARGS__)
#else
#   define VK_LOG(...) ((void)0)
#endif

