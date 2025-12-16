// @pjb: Vulkan rendering backend - driver interface
// This header is designed to be includable from C code (like tr_init.c)
// It assumes tr_local.h has already been included for type definitions
#ifndef VK_DRIVER_H
#define VK_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Driver initialization
//=============================================================================

void VkDrv_DriverInit(void);

//=============================================================================
// GFX_* function implementations
//=============================================================================

// Lifecycle
void            VkDrv_Shutdown(void);
void            VkDrv_UnbindResources(void);
size_t          VkDrv_LastError(void);

// Info
void            VkDrv_GfxInfo(void);
int             VkDrv_SumOfUsedImages(void);

// Pixels/readback
void            VkDrv_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFmt, void* dest);
void            VkDrv_ReadDepth(int x, int y, int width, int height, float* dest);
void            VkDrv_ReadStencil(int x, int y, int width, int height, byte* dest);

// Images
void            VkDrv_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap);
void            VkDrv_DeleteImage(const image_t* image);
void            VkDrv_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty);
imageFormat_t   VkDrv_GetImageFormat(const image_t* image);
void            VkDrv_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

// Drawing 2D
void            VkDrv_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color);

// Clearing
void            VkDrv_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth);

// Matrices
void            VkDrv_SetProjection(const float* projMatrix);
void            VkDrv_GetProjection(float* projMatrix);
void            VkDrv_SetModelView(const float* modelViewMatrix);
void            VkDrv_GetModelView(float* modelViewMatrix);

// Viewport/state
void            VkDrv_SetViewport(int left, int top, int width, int height);
void            VkDrv_Flush(void);
void            VkDrv_SetState(unsigned long stateMask);
void            VkDrv_ResetState2D(void);
void            VkDrv_ResetState3D(void);
void            VkDrv_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane);
void            VkDrv_SetDepthRange(float minRange, float maxRange);
void            VkDrv_SetDrawBuffer(int buffer);

// Frame
void            VkDrv_EndFrame(void);
void            VkDrv_MakeCurrent(qboolean current);

// Shadows
void            VkDrv_ShadowSilhouette(const float* edges, int edgeCount);
void            VkDrv_ShadowFinish(void);

// Skybox
void            VkDrv_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint);

// Beam
void            VkDrv_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs);

// Stage drawing
void            VkDrv_DrawStageGeneric(const shaderCommands_t* input);
void            VkDrv_DrawStageVertexLitTexture(const shaderCommands_t* input);
void            VkDrv_DrawStageLightmappedMultitexture(const shaderCommands_t* input);

// Tessellation
void            VkDrv_BeginTessellate(const shaderCommands_t* input);
void            VkDrv_EndTessellate(const shaderCommands_t* input);

// Debug
void            VkDrv_DebugDrawAxis(void);
void            VkDrv_DebugDrawNormals(const shaderCommands_t* input);
void            VkDrv_DebugDrawTris(const shaderCommands_t* input);
void            VkDrv_DebugSetOverdrawMeasureEnabled(qboolean enabled);
void            VkDrv_DebugSetTextureMode(const char* mode);
void            VkDrv_DebugDrawPolygon(int color, int numPoints, const float* points);

#ifdef __cplusplus
}
#endif

#endif // VK_DRIVER_H
