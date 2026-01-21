#pragma once

#include "vk_common.h"

//----------------------------------------------------------------------------
// Frame data (double-buffered)
//----------------------------------------------------------------------------
struct vkFrameData_t {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence renderFence;
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    uint32_t frameIndex;
};

//----------------------------------------------------------------------------
// Circular buffer for dynamic data
//----------------------------------------------------------------------------
struct vkCircularBuffer_t {
    VkBuffer buffer;
    VmaAllocation allocation;
    void* mappedData;          // Persistent mapping
    uint32_t size;
    uint32_t currentOffset;
    uint32_t nextOffset;
    uint32_t frameOffset[VK_MAX_FRAMES_IN_FLIGHT];  // Per-frame regions
};

//----------------------------------------------------------------------------
// Tessellation buffers (analogous to D3D11's d3dTessBuffers_t)
//----------------------------------------------------------------------------
struct vkTessStageBuffers_t {
    vkCircularBuffer_t texCoords[NUM_TEXTURE_BUNDLES];
    vkCircularBuffer_t colors;
};

struct vkTessFogBuffers_t {
    vkCircularBuffer_t texCoords;
    vkCircularBuffer_t colors;
};

struct vkTessLightProjBuffers_t {
    vkCircularBuffer_t indexes;
    vkCircularBuffer_t texCoords;
    vkCircularBuffer_t colors;
};

struct vkTessBuffers_t {
    vkCircularBuffer_t indexes;
    vkCircularBuffer_t xyz;
    vkTessStageBuffers_t stages[MAX_SHADER_STAGES];
    vkTessLightProjBuffers_t dlights[MAX_DLIGHTS];
    vkTessFogBuffers_t fog;
};

//----------------------------------------------------------------------------
// Uniform buffers
//----------------------------------------------------------------------------
struct vkUniformBuffers_t {
    vkCircularBuffer_t viewVS;
    vkCircularBuffer_t viewPS;
    vkCircularBuffer_t stage;
    vkCircularBuffer_t skyboxVS;
    vkCircularBuffer_t skyboxPS;
};

//----------------------------------------------------------------------------
// Descriptor sets
//----------------------------------------------------------------------------
struct vkDescriptorSets_t {
    VkDescriptorSet viewSets[VK_MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet currentViewSet;
    VkDescriptorSet stageSets[VK_MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet currentStageSet;
    // Texture sets allocated on-demand per image
};

//----------------------------------------------------------------------------
// Draw state
//----------------------------------------------------------------------------
struct vkDrawState_t {
    // Frame data
    vkFrameData_t frames[VK_MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame;
    uint32_t imageIndex;  // Current swapchain image

    // Buffers
    vkTessBuffers_t tessBuffers;
    vkUniformBuffers_t uniformBuffers;

    // Descriptor sets
    vkDescriptorSets_t descriptorSets;

    // Render pass state
    qboolean inRenderPass;
};

//----------------------------------------------------------------------------
// Global draw state
//----------------------------------------------------------------------------
extern vkDrawState_t g_vkDraw;

//----------------------------------------------------------------------------
// Command buffer management
//----------------------------------------------------------------------------

// Create frame resources
void VK_CreateFrameResources();

// Destroy frame resources
void VK_DestroyFrameResources();

// Begin frame
void VK_BeginFrame();

// End frame (submit and present)
#ifdef __cplusplus
extern "C" {
#endif
void VK_EndFrame();
#ifdef __cplusplus
}
#endif

// Get current command buffer
VkCommandBuffer VK_GetCurrentCommandBuffer();

// Flush GPU (wait for completion)
void VK_FlushGPU();

//----------------------------------------------------------------------------
// Circular buffer management
//----------------------------------------------------------------------------

// Create circular buffer
void VK_CreateCircularBuffer(vkCircularBuffer_t* buf, uint32_t size, VkBufferUsageFlags usage);

// Destroy circular buffer
void VK_DestroyCircularBuffer(vkCircularBuffer_t* buf);

// Update circular buffer
void VK_UpdateCircularBuffer(vkCircularBuffer_t* buf, const void* data, uint32_t dataSize);

// Reset circular buffer for new frame
void VK_ResetCircularBuffer(vkCircularBuffer_t* buf, uint32_t frameIndex);

//----------------------------------------------------------------------------
// Uniform buffer management
//----------------------------------------------------------------------------

// Create uniform buffers
void VK_CreateUniformBuffers();

// Destroy uniform buffers
void VK_DestroyUniformBuffers();

// Update view VS uniform
void VK_UpdateViewVSUniform();

// Update view PS uniform
void VK_UpdateViewPSUniform();

//----------------------------------------------------------------------------
// Descriptor set management
//----------------------------------------------------------------------------

// Create descriptor sets
void VK_CreateDescriptorSets();

// Update view descriptor set
void VK_UpdateViewDescriptorSet(uint32_t frameIndex);

// Update stage descriptor set
void VK_UpdateStageDescriptorSet(uint32_t frameIndex);

//----------------------------------------------------------------------------
// Tessellation buffer management
//----------------------------------------------------------------------------

// Create tessellation buffers
void VK_CreateTessellationBuffers();

// Destroy tessellation buffers
void VK_DestroyTessellationBuffers();

// Update tessellation buffers
void VK_UpdateTessBuffers(const shaderCommands_t* input, qboolean needDlights, qboolean needFog);

//----------------------------------------------------------------------------
// Drawing functions (internal)
//----------------------------------------------------------------------------

// Draw indexed primitives
void VK_DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset);

// Set vertex buffers for single-texture stage
void VK_SetVertexBuffersST(const vkTessStageBuffers_t* stage);

// Set vertex buffers for multi-texture stage
void VK_SetVertexBuffersMT(const vkTessStageBuffers_t* stage);

// Draw dynamic lights
void VK_DrawDynamicLights(const shaderCommands_t* input);

// Draw fog pass
void VK_DrawFog(const shaderCommands_t* input);

