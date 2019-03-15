#include "vk_common.h"
#include "vk_driver.h"


void VkSetupVideoConfig()
{
	// TODO: Fix me!
	const char* vulkanVersionStr = "";
}

void VkInitDrawState()
{
	// TODO: fix me!
}

VK_PUBLIC void VKDrv_DriverInit(void)
{
	GFX_Shutdown				= VKDrv_Shutdown;
	GFX_UnbindResources			= VKDrv_UnbindResources;
	GFX_LastError				= VKDrv_LastError;
	GFX_ReadPixels = VKDrv_ReadPixels;
	GFX_ReadDepth = VKDrv_ReadDepth;
	GFX_ReadStencil = VKDrv_ReadStencil;
	GFX_CreateImage = VKDrv_CreateImage;
	GFX_DeleteImage = VKDrv_DeleteImage;
	GFX_UpdateCinematic = VKDrv_UpdateCinematic;
	GFX_DrawImage = VKDrv_DrawImage;
	GFX_GetImageFormat = VKDrv_GetImageFormat;
	GFX_SetGamma = VKDrv_SetGamma;
	GFX_GetFrameImageMemoryUsage = VKDrv_SumOfUsedImages;
	GFX_GraphicsInfo = VKDrv_GfxInfo;
	GFX_Clear = VKDrv_Clear;
	GFX_SetProjectionMatrix = VKDrv_SetProjection;
	GFX_GetProjectionMatrix = VKDrv_GetProjection;
	GFX_SetModelViewMatrix = VKDrv_SetModelView;
	GFX_GetModelViewMatrix = VKDrv_GetModelView;
	GFX_SetViewport = VKDrv_SetViewport;
	GFX_Flush = VKDrv_Flush;
	GFX_SetState = VKDrv_SetState;
	GFX_ResetState2D = VKDrv_ResetState2D;
	GFX_ResetState3D = VKDrv_ResetState3D;
	GFX_SetPortalRendering = VKDrv_SetPortalRendering;
	GFX_SetDepthRange = VKDrv_SetDepthRange;
	GFX_SetDrawBuffer = VKDrv_SetDrawBuffer;
	GFX_EndFrame = VKDrv_EndFrame;
	GFX_MakeCurrent = VKDrv_MakeCurrent;
	GFX_ShadowSilhouette = VKDrv_ShadowSilhouette;
	GFX_ShadowFinish = VKDrv_ShadowFinish;
	GFX_DrawSkyBox = VKDrv_DrawSkyBox;
	GFX_DrawBeam = VKDrv_DrawBeam;
	GFX_DrawStageGeneric = VKDrv_DrawStageGeneric;
	GFX_DrawStageVertexLitTexture = VKDrv_DrawStageVertexLitTexture;
	GFX_DrawStageLightmappedMultitexture = VKDrv_DrawStageLightmappedMultitexture;
	GFX_DebugDrawAxis = VKDrv_DebugDrawAxis;
	GFX_DebugDrawTris = VKDrv_DebugDrawTris;
	GFX_DebugDrawNormals = VKDrv_DebugDrawNormals;
	GFX_DebugSetOverdrawMeasureEnabled = VKDrv_DebugSetOverdrawMeasureEnabled;
	GFX_DebugSetTextureMode = VKDrv_DebugSetTextureMode;
	GFX_DebugDrawPolygon = VKDrv_DebugDrawPolygon;

	// Init VK Window

	// Init Draw State
	VkInitDrawState();
	//Setup vdConfig global
	VkSetupVideoConfig();
}