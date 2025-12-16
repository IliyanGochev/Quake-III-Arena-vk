// @pjb: Vulkan rendering backend - draw commands
#ifndef VK_DRAW_H
#define VK_DRAW_H

#include "vk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Functions
//=============================================================================

void VkDraw_Init(void);
void VkDraw_Shutdown(void);

// Clear
void VkDraw_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth);

// Begin a new frame (acquire swapchain image, begin command buffer)
qboolean VkDraw_BeginFrame(void);

// 2D drawing
void VkDraw_Image(const image_t* image, const float* coords, const float* texcoords, const float* color);

// 3D drawing
void VkDraw_SkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint);
void VkDraw_Beam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs);

// Stage iteration
void VkDraw_StageGeneric(const shaderCommands_t* input);
void VkDraw_StageVertexLitTexture(const shaderCommands_t* input);
void VkDraw_StageLightmappedMultitexture(const shaderCommands_t* input);

// Tessellation
void VkDraw_BeginTessellate(const shaderCommands_t* input);
void VkDraw_EndTessellate(const shaderCommands_t* input);

// Shadows
void VkDraw_ShadowSilhouette(const float* edges, int edgeCount);
void VkDraw_ShadowFinish(void);

#ifdef __cplusplus
}
#endif

#endif // VK_DRAW_H
