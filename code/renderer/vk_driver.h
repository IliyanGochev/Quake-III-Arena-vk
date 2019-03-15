#pragma once

#ifdef __cplusplus 
	extern "C" {
#endif

	void VKDrv_DriverInit(void);

	// Driver Entry Points
	void VKDrv_Shutdown(void);
	void VKDrv_UnbindResources(void);
	size_t VKDrv_LastError(void);
	void VKDrv_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFormat, void* dest);
	void VKDrv_ReadDepth(int x, int y, int width, int height, float* dest);
	void VKDrv_ReadDepth(int x, int y, int width, int height, float* dest);
	void VKDrv_ReadStencil(int x, int y, int width, int height, byte* dest);
	void VKDrv_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap);
	void VKDrv_DeleteImage(const image_t* image);
	void VKDrv_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty);
	void VKDrv_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color);
	imageFormat_t VKDrv_GetImageFormat(const image_t* image);
	void VKDrv_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);
	 int VKDrv_SumOfUsedImages(void);
	void VKDrv_GfxInfo(void);
	void VKDrv_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth);
	void VKDrv_SetProjection(const float* projMatrix);
	void VKDrv_GetProjection(float* projMatrix);
	void VKDrv_SetModelView(const float* modelViewMatrix);
	void VKDrv_GetModelView(float* modelViewMatrix);
	void VKDrv_SetViewport(int left, int top, int width, int height);
	void VKDrv_Flush(void);
	void VKDrv_SetState(unsigned long stateMask);
	void VKDrv_ResetState2D(void);
	void VKDrv_ResetState3D(void);
	void VKDrv_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane);
	void VKDrv_SetDepthRange(float minRange, float maxRange);
	void VKDrv_SetDrawBuffer(int buffer);
	void VKDrv_EndFrame(void);
	void VKDrv_MakeCurrent(qboolean current);
	void VKDrv_ShadowSilhouette(const float* edges, int edgeCount);
	void VKDrv_ShadowFinish(void);
	void VKDrv_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint);
	void VKDrv_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs);
	void VKDrv_DrawStageGeneric(const shaderCommands_t* input);
	void VKDrv_DrawStageVertexLitTexture(const shaderCommands_t* input);
	void VKDrv_DrawStageLightmappedMultitexture(const shaderCommands_t* input);
	void VKDrv_DebugDrawAxis(void);
	void VKDrv_DebugDrawTris(const shaderCommands_t* input);
	void VKDrv_DebugDrawNormals(const shaderCommands_t* input);
	void VKDrv_DebugSetOverdrawMeasureEnabled(qboolean enabled);
	void VKDrv_DebugSetTextureMode(const char* mode);
	void VKDrv_DebugDrawPolygon(int color, int numPoints, const float* points);
#ifdef  __cplusplus
} // extern C
#endif
