// @pjb: Vulkan rendering backend - buffer management
#ifndef VK_BUFFERS_H
#define VK_BUFFERS_H

#include "vk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Ring buffer for dynamic allocations (C++ only due to VMA)
//=============================================================================

#ifdef __cplusplus
typedef struct vkRingBuffer_s {
    VkBuffer        buffer;
    VmaAllocation   allocation;
    void*           mapped;
    VkDeviceSize    size;
    VkDeviceSize    currentOffset;
    VkDeviceSize    frameStart[VK_MAX_FRAMES_IN_FLIGHT];
} vkRingBuffer_t;
#endif

//=============================================================================
// Buffer allocation result
//=============================================================================

typedef struct vkBufferAlloc_s {
    VkBuffer        buffer;
    VkDeviceSize    offset;
    void*           data;
} vkBufferAlloc_t;

//=============================================================================
// Functions
//=============================================================================

void VkBuffers_Init(void);
void VkBuffers_Shutdown(void);

// Called at end of frame to advance ring buffer positions
void VkBuffers_ResetFrame(void);

// Allocate from vertex ring buffer
vkBufferAlloc_t VkBuffers_AllocVertex(VkDeviceSize size);

// Allocate from index ring buffer
vkBufferAlloc_t VkBuffers_AllocIndex(VkDeviceSize size);

// Allocate from uniform ring buffer
vkBufferAlloc_t VkBuffers_AllocUniform(VkDeviceSize size);

// Get the vertex buffer handle
VkBuffer VkBuffers_GetVertexBuffer(void);

// Get the index buffer handle
VkBuffer VkBuffers_GetIndexBuffer(void);

// Get the uniform buffer handle
VkBuffer VkBuffers_GetUniformBuffer(void);

#ifdef __cplusplus
}
#endif

#endif // VK_BUFFERS_H
