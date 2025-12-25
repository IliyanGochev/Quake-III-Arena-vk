// @pjb: Vulkan rendering backend - state management
#ifndef VK_STATE_H
#define VK_STATE_H

#include "vk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// View state (uniforms)
//=============================================================================

typedef struct vkViewState_s {
    float projectionMatrix[16];
    float modelViewMatrix[16];
    float depthRange[2];
    float eyePos[3];        // Camera position (for skybox centering)
    float clipPlane[4];
    float alphaClip[2];     // [threshold, enable]

    qboolean projectionDirty;
    qboolean modelViewDirty;
    qboolean clipPlaneDirty;
    qboolean alphaClipDirty;
} vkViewState_t;

//=============================================================================
// Pipeline state
//=============================================================================

typedef struct vkPipelineState_s {
    unsigned long stateBits;
    int cullMode;
    qboolean portalRendering;

    // Viewport/scissor
    int viewportX, viewportY;
    int viewportWidth, viewportHeight;
    float depthMin, depthMax;

    // Current bound resources
    VkPipeline currentPipeline;
    VkDescriptorSet currentTextureSet;
} vkPipelineState_t;

extern vkViewState_t g_vkViewState;
extern vkPipelineState_t g_vkPipelineState;

//=============================================================================
// Functions
//=============================================================================

void VkState_Init(void);
void VkState_Shutdown(void);

// State bits
void VkState_SetState(unsigned long stateBits);
unsigned long VkState_GetState(void);

// Matrices
void VkState_SetProjection(const float* matrix);
void VkState_GetProjection(float* matrix);
void VkState_SetModelView(const float* matrix);
void VkState_GetModelView(float* matrix);

// Viewport
void VkState_SetViewport(int x, int y, int width, int height);
void VkState_SetDepthRange(float minRange, float maxRange);

// Eye position (for skybox centering)
void VkState_SetEyePos(const float* eyePos);

// Portal/clip
void VkState_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane);

// 2D/3D modes
void VkState_Reset2D(void);
void VkState_Reset3D(void);

// Pipeline helpers
VkPipeline VkState_GetPipeline(unsigned long stateBits, int cullMode, qboolean isMirror,
                               qboolean isMultitextured, qboolean isSkybox);
VkPipeline VkState_Get2DPipeline(void);
VkPipeline VkState_GetSkyboxPipeline(void);
void VkState_ResetPipelines(void);  // Destroys and clears all cached pipelines

// Descriptor set helpers
VkDescriptorSetLayout VkState_GetTextureSetLayout(void);
VkDescriptorSetLayout VkState_GetUniformSetLayout(void);
VkPipelineLayout VkState_GetPipelineLayout(void);

// UBO management (legacy static buffer - deprecated)
void VkState_UpdateUniforms(void);
VkBuffer VkState_GetVSUniformBuffer(void);
VkBuffer VkState_GetPSUniformBuffer(void);
VkDeviceSize VkState_GetVSUniformSize(void);
VkDeviceSize VkState_GetPSUniformSize(void);

// Dynamic UBO allocation from ring buffer (use this for per-draw uniforms)
// Returns offsets into the uniform ring buffer for use with dynamic UBO binding
qboolean VkState_AllocDynamicUniforms(uint32_t* outVsOffset, uint32_t* outPsOffset);

#ifdef __cplusplus
}
#endif

#endif // VK_STATE_H
