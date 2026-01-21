#pragma once

// Forward declarations - avoid including vk_common.h to keep this header C-compatible
// The types below are defined in tr_local.h which tr_init.c already includes
#ifndef byte
typedef unsigned char byte;
#endif

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------
// Main driver initialization
//----------------------------------------------------------------------------
void VK_DriverInit(void);

//----------------------------------------------------------------------------
// Driver entry points (GFX_* interface implementation)
//----------------------------------------------------------------------------

void VK_Shutdown(void);
void VK_UnbindResources(void);
size_t VK_LastError(void);
void VK_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFmt, void* dest);
void VK_ReadDepth(int x, int y, int width, int height, float* dest);
void VK_ReadStencil(int x, int y, int width, int height, byte* dest);
void VK_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap);
void VK_DeleteImage(const image_t* image);
void VK_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty);
void VK_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color);
imageFormat_t VK_GetImageFormat(const image_t* image);
void VK_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);
int VK_GetFrameImageMemoryUsage(void);
void VK_GraphicsInfo(void);
void VK_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth);
void VK_SetProjectionMatrix(const float* projMatrix);
void VK_GetProjectionMatrix(float* projMatrix);
void VK_SetModelViewMatrix(const float* modelViewMatrix);
void VK_GetModelViewMatrix(float* modelViewMatrix);
void VK_SetViewport(int left, int top, int width, int height);
void VK_Flush(void);
void VK_SetState(unsigned long stateMask);
void VK_ResetState2D(void);
void VK_ResetState3D(void);
void VK_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane);
void VK_SetDepthRange(float minRange, float maxRange);
void VK_SetDrawBuffer(int buffer);
void VK_EndFrame(void);
void VK_MakeCurrent(qboolean current);
void VK_ShadowSilhouette(const float* edges, int edgeCount);
void VK_ShadowFinish(void);
void VK_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint);
void VK_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs);
void VK_DrawStageGeneric(const shaderCommands_t* input);
void VK_DrawStageVertexLitTexture(const shaderCommands_t* input);
void VK_DrawStageLightmappedMultitexture(const shaderCommands_t* input);
void VK_BeginTessellate(const shaderCommands_t* input);
void VK_EndTessellate(const shaderCommands_t* input);
void VK_DebugDrawAxis(void);
void VK_DebugDrawNormals(const shaderCommands_t* input);
void VK_DebugDrawTris(const shaderCommands_t* input);
void VK_DebugSetOverdrawMeasureEnabled(qboolean enabled);
void VK_DebugSetTextureMode(const char* mode);
void VK_DebugDrawPolygon(int color, int numPoints, const float* points);

//----------------------------------------------------------------------------
// Internal driver functions
//----------------------------------------------------------------------------

// Setup video configuration
void VK_SetupVideoConfig(void);

#ifdef __cplusplus
}
#endif

