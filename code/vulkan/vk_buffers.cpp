// @pjb: Vulkan rendering backend - buffer management implementation

#include "vk_buffers.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================

static vkRingBuffer_t s_vertexBuffer;
static vkRingBuffer_t s_indexBuffer;
static vkRingBuffer_t s_uniformBuffer;

//=============================================================================
// Ring buffer helpers
//=============================================================================

static qboolean CreateRingBuffer(vkRingBuffer_t* ring, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocResult;
    VkResult result = vmaCreateBuffer(g_vmaAllocator, &bufferInfo, &allocInfo,
        &ring->buffer, &ring->allocation, &allocResult);

    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: Failed to create ring buffer: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    ring->mapped = allocResult.pMappedData;
    ring->size = size;
    ring->currentOffset = 0;

    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        ring->frameStart[i] = 0;
    }

    return qtrue;
}

static void DestroyRingBuffer(vkRingBuffer_t* ring)
{
    if (ring->buffer) {
        vmaDestroyBuffer(g_vmaAllocator, ring->buffer, ring->allocation);
        ring->buffer = VK_NULL_HANDLE;
        ring->allocation = VK_NULL_HANDLE;
        ring->mapped = NULL;
    }
}

static vkBufferAlloc_t AllocFromRing(vkRingBuffer_t* ring, VkDeviceSize size, VkDeviceSize alignment)
{
    vkBufferAlloc_t result = {};

    // Align the current offset
    VkDeviceSize alignedOffset = (ring->currentOffset + alignment - 1) & ~(alignment - 1);

    // Check if we need to wrap around
    if (alignedOffset + size > ring->size) {
        // Wrap to beginning
        alignedOffset = 0;
    }

    // Check if we would overwrite data from a frame still in flight
    // Simple check: just make sure we have space
    if (alignedOffset + size > ring->size) {
        Com_Printf("WARNING: Ring buffer overflow\n");
        return result;
    }

    result.buffer = ring->buffer;
    result.offset = alignedOffset;
    result.data = (byte*)ring->mapped + alignedOffset;

    ring->currentOffset = alignedOffset + size;

    return result;
}

//=============================================================================
// Public API
//=============================================================================

void VkBuffers_Init(void)
{
    Com_Memset(&s_vertexBuffer, 0, sizeof(s_vertexBuffer));
    Com_Memset(&s_indexBuffer, 0, sizeof(s_indexBuffer));
    Com_Memset(&s_uniformBuffer, 0, sizeof(s_uniformBuffer));

    if (!CreateRingBuffer(&s_vertexBuffer, VK_VERTEX_BUFFER_SIZE,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        ri.Error(ERR_FATAL, "Failed to create vertex ring buffer");
    }

    if (!CreateRingBuffer(&s_indexBuffer, VK_INDEX_BUFFER_SIZE,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        ri.Error(ERR_FATAL, "Failed to create index ring buffer");
    }

    if (!CreateRingBuffer(&s_uniformBuffer, VK_UNIFORM_BUFFER_SIZE,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        ri.Error(ERR_FATAL, "Failed to create uniform ring buffer");
    }

    Com_Printf("Vulkan buffers initialized: vertex=%dMB, index=%dMB, uniform=%dKB\n",
        VK_VERTEX_BUFFER_SIZE / (1024 * 1024),
        VK_INDEX_BUFFER_SIZE / (1024 * 1024),
        VK_UNIFORM_BUFFER_SIZE / 1024);
}

void VkBuffers_Shutdown(void)
{
    DestroyRingBuffer(&s_vertexBuffer);
    DestroyRingBuffer(&s_indexBuffer);
    DestroyRingBuffer(&s_uniformBuffer);
}

void VkBuffers_ResetFrame(void)
{
    // Record where this frame started in each buffer
    // Next frame can safely overwrite up to this point once the frame completes
    uint32_t nextFrame = (vk.currentFrame + 1) % VK_MAX_FRAMES_IN_FLIGHT;

    s_vertexBuffer.frameStart[nextFrame] = s_vertexBuffer.currentOffset;
    s_indexBuffer.frameStart[nextFrame] = s_indexBuffer.currentOffset;
    s_uniformBuffer.frameStart[nextFrame] = s_uniformBuffer.currentOffset;
}

vkBufferAlloc_t VkBuffers_AllocVertex(VkDeviceSize size)
{
    // Vertices should be aligned to 4 bytes at minimum
    return AllocFromRing(&s_vertexBuffer, size, 4);
}

vkBufferAlloc_t VkBuffers_AllocIndex(VkDeviceSize size)
{
    // Indices (16-bit) should be aligned to 2 bytes
    return AllocFromRing(&s_indexBuffer, size, 2);
}

vkBufferAlloc_t VkBuffers_AllocUniform(VkDeviceSize size)
{
    // Uniform buffers have strict alignment requirements
    VkDeviceSize alignment = vk.deviceProperties.limits.minUniformBufferOffsetAlignment;
    if (alignment < 256) alignment = 256; // Common requirement
    return AllocFromRing(&s_uniformBuffer, size, alignment);
}

VkBuffer VkBuffers_GetVertexBuffer(void)
{
    return s_vertexBuffer.buffer;
}

VkBuffer VkBuffers_GetIndexBuffer(void)
{
    return s_indexBuffer.buffer;
}

VkBuffer VkBuffers_GetUniformBuffer(void)
{
    return s_uniformBuffer.buffer;
}
