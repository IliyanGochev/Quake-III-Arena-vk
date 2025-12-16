// @pjb: Vulkan rendering backend - driver implementation

#include "vk_common.h"  // Must be first - includes Vulkan headers
#include "vk_driver.h"
#include "vk_state.h"
#include "vk_image.h"
#include "vk_buffers.h"
#include "vk_shaders.h"
#include "vk_draw.h"
#include "../win32/win_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//=============================================================================
// Driver initialization
//=============================================================================

void VkDrv_DriverInit(void)
{
    Com_Printf("----- Vulkan Driver Init -----\n");

    // Clear context
    Com_Memset(&vk, 0, sizeof(vk));

    // Create window first
    if (!VkWnd_Init()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan window");
        return;
    }

    // Load Vulkan library
    if (!Vk_LoadLibrary()) {
        ri.Error(ERR_FATAL, "Failed to load Vulkan library");
        return;
    }

    // Create instance
    if (!Vk_CreateInstance()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan instance");
        return;
    }

    // Create surface using our window
    HWND hwnd = VkWnd_GetHandle();
    if (!Vk_CreateSurface(hwnd)) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan surface");
        return;
    }

    // Select physical device
    if (!Vk_SelectPhysicalDevice()) {
        ri.Error(ERR_FATAL, "Failed to select Vulkan physical device");
        return;
    }

    // Create logical device
    if (!Vk_CreateDevice()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan logical device");
        return;
    }

    // Create swapchain
    if (!Vk_CreateSwapchain()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan swapchain");
        return;
    }

    // Create depth buffer
    if (!Vk_CreateDepthBuffer()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan depth buffer");
        return;
    }

    // Create render pass
    if (!Vk_CreateRenderPass()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan render pass");
        return;
    }

    // Create framebuffers
    if (!Vk_CreateFramebuffers()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan framebuffers");
        return;
    }

    // Create command pools
    if (!Vk_CreateCommandPools()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan command pools");
        return;
    }

    // Create sync objects
    if (!Vk_CreateSyncObjects()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan sync objects");
        return;
    }

    // Create descriptor pool
    if (!Vk_CreateDescriptorPool()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan descriptor pool");
        return;
    }

    // Create pipeline cache
    if (!Vk_CreatePipelineCache()) {
        ri.Error(ERR_FATAL, "Failed to create Vulkan pipeline cache");
        return;
    }

    // Initialize subsystems
    VkState_Init();
    VkImage_Init();
    VkBuffers_Init();
    VkShaders_Init();
    VkDraw_Init();

    // Assign function pointers to the abstraction layer
    GFX_Shutdown = VkDrv_Shutdown;
    GFX_UnbindResources = VkDrv_UnbindResources;
    GFX_LastError = VkDrv_LastError;
    GFX_ReadPixels = VkDrv_ReadPixels;
    GFX_ReadDepth = VkDrv_ReadDepth;
    GFX_ReadStencil = VkDrv_ReadStencil;
    GFX_CreateImage = VkDrv_CreateImage;
    GFX_DeleteImage = VkDrv_DeleteImage;
    GFX_UpdateCinematic = VkDrv_UpdateCinematic;
    GFX_DrawImage = VkDrv_DrawImage;
    GFX_GetImageFormat = VkDrv_GetImageFormat;
    GFX_SetGamma = VkDrv_SetGamma;
    GFX_GetFrameImageMemoryUsage = VkDrv_SumOfUsedImages;
    GFX_GraphicsInfo = VkDrv_GfxInfo;
    GFX_Clear = VkDrv_Clear;
    GFX_SetProjectionMatrix = VkDrv_SetProjection;
    GFX_GetProjectionMatrix = VkDrv_GetProjection;
    GFX_SetModelViewMatrix = VkDrv_SetModelView;
    GFX_GetModelViewMatrix = VkDrv_GetModelView;
    GFX_SetViewport = VkDrv_SetViewport;
    GFX_Flush = VkDrv_Flush;
    GFX_SetState = VkDrv_SetState;
    GFX_ResetState2D = VkDrv_ResetState2D;
    GFX_ResetState3D = VkDrv_ResetState3D;
    GFX_SetPortalRendering = VkDrv_SetPortalRendering;
    GFX_SetDepthRange = VkDrv_SetDepthRange;
    GFX_SetDrawBuffer = VkDrv_SetDrawBuffer;
    GFX_EndFrame = VkDrv_EndFrame;
    GFX_MakeCurrent = VkDrv_MakeCurrent;
    GFX_ShadowSilhouette = VkDrv_ShadowSilhouette;
    GFX_ShadowFinish = VkDrv_ShadowFinish;
    GFX_DrawSkyBox = VkDrv_DrawSkyBox;
    GFX_DrawBeam = VkDrv_DrawBeam;
    GFX_DrawStageGeneric = VkDrv_DrawStageGeneric;
    GFX_DrawStageVertexLitTexture = VkDrv_DrawStageVertexLitTexture;
    GFX_DrawStageLightmappedMultitexture = VkDrv_DrawStageLightmappedMultitexture;
    // Note: GFX_BeginTessellate/GFX_EndTessellate not defined in tr_layer.c and not called anywhere
    GFX_DebugDrawAxis = VkDrv_DebugDrawAxis;
    GFX_DebugDrawNormals = VkDrv_DebugDrawNormals;
    GFX_DebugDrawTris = VkDrv_DebugDrawTris;
    GFX_DebugSetOverdrawMeasureEnabled = VkDrv_DebugSetOverdrawMeasureEnabled;
    GFX_DebugSetTextureMode = VkDrv_DebugSetTextureMode;
    GFX_DebugDrawPolygon = VkDrv_DebugDrawPolygon;

    vk.initialized = qtrue;

    Com_Printf("----- Vulkan Driver Init Complete -----\n");
}

//=============================================================================
// Shutdown
//=============================================================================

void VkDrv_Shutdown(void)
{
    Com_Printf("Shutting down Vulkan...\n");

    if (!vk.initialized) {
        return;
    }

    // Wait for GPU to finish
    if (vk.device) {
        qvkDeviceWaitIdle(vk.device);
    }

    // Shutdown subsystems (reverse order)
    VkDraw_Shutdown();
    VkShaders_Shutdown();
    VkBuffers_Shutdown();
    VkImage_Shutdown();
    VkState_Shutdown();

    // Destroy Vulkan objects
    Vk_DestroyPipelineCache();
    Vk_DestroyDescriptorPool();
    Vk_DestroySyncObjects();
    Vk_DestroyCommandPools();
    Vk_DestroyFramebuffers();
    Vk_DestroyRenderPass();
    Vk_DestroyDepthBuffer();
    Vk_DestroySwapchain();
    Vk_DestroyDevice();
    Vk_DestroySurface();
    Vk_DestroyInstance();
    Vk_UnloadLibrary();

    Com_Memset(&vk, 0, sizeof(vk));
}

void VkDrv_UnbindResources(void)
{
    // Vulkan doesn't need explicit unbinding like OpenGL
}

size_t VkDrv_LastError(void)
{
    return (size_t)vk.lastError;
}

//=============================================================================
// Info
//=============================================================================

void VkDrv_GfxInfo(void)
{
    Com_Printf("\nVulkan Graphics Info:\n");
    Com_Printf("  Device: %s\n", vk.deviceProperties.deviceName);
    Com_Printf("  API Version: %d.%d.%d\n",
        VK_VERSION_MAJOR(vk.deviceProperties.apiVersion),
        VK_VERSION_MINOR(vk.deviceProperties.apiVersion),
        VK_VERSION_PATCH(vk.deviceProperties.apiVersion));
    Com_Printf("  Driver Version: %d.%d.%d\n",
        VK_VERSION_MAJOR(vk.deviceProperties.driverVersion),
        VK_VERSION_MINOR(vk.deviceProperties.driverVersion),
        VK_VERSION_PATCH(vk.deviceProperties.driverVersion));
    Com_Printf("  Swapchain: %dx%d, %d images\n",
        vk.swapchain.extent.width, vk.swapchain.extent.height, vk.swapchain.imageCount);

    const char* deviceType = "Unknown";
    switch (vk.deviceProperties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "CPU"; break;
        default: break;
    }
    Com_Printf("  Device Type: %s\n", deviceType);
}

int VkDrv_SumOfUsedImages(void)
{
    return VkImage_GetMemoryUsage();
}

//=============================================================================
// Readback (stub implementations - full implementation later)
//=============================================================================

void VkDrv_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFmt, void* dest)
{
    // TODO: Implement pixel readback using staging buffer
    Com_Memset(dest, 0, width * height * 4);
}

void VkDrv_ReadDepth(int x, int y, int width, int height, float* dest)
{
    // TODO: Implement depth readback
    Com_Memset(dest, 0, width * height * sizeof(float));
}

void VkDrv_ReadStencil(int x, int y, int width, int height, byte* dest)
{
    // TODO: Implement stencil readback
    Com_Memset(dest, 0, width * height);
}

//=============================================================================
// Gamma
//=============================================================================

void VkDrv_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256])
{
    // TODO: Implement gamma via shader or swapchain
    // For now, gamma is not supported in Vulkan backend
}

//=============================================================================
// Frame management
//=============================================================================

void VkDrv_EndFrame(void)
{
    if (!vk.inFrame) {
        return;
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];

    // End render pass if active
    if (frame->inRenderPass) {
        qvkCmdEndRenderPass(frame->commandBuffer);
        frame->inRenderPass = qfalse;
    }

    // End command buffer recording
    if (frame->commandBufferRecording) {
        VkResult result = qvkEndCommandBuffer(frame->commandBuffer);
        if (result != VK_SUCCESS) {
            Com_Printf("ERROR: vkEndCommandBuffer failed: %s\n", Vk_ResultString(result));
        }
        frame->commandBufferRecording = qfalse;
    }

    // Submit command buffer
    VkSemaphore waitSemaphores[] = { frame->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frame->renderFinishedSemaphore };

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult result = qvkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, frame->inFlightFence);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkQueueSubmit failed: %s\n", Vk_ResultString(result));
    }

    // Present
    VkSwapchainKHR swapchains[] = { vk.swapchain.handle };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &vk.currentImageIndex;

    result = qvkQueuePresentKHR(vk.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        Vk_RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkQueuePresentKHR failed: %s\n", Vk_ResultString(result));
    }

    // Advance to next frame
    vk.currentFrame = (vk.currentFrame + 1) % VK_MAX_FRAMES_IN_FLIGHT;
    vk.inFrame = qfalse;

    // Reset buffer allocators for next frame
    VkBuffers_ResetFrame();
}

void VkDrv_MakeCurrent(qboolean current)
{
    // Vulkan doesn't have a "make current" concept like OpenGL
    // This is a no-op
}

void VkDrv_Flush(void)
{
    if (vk.device) {
        qvkDeviceWaitIdle(vk.device);
    }
}

//=============================================================================
// State
//=============================================================================

void VkDrv_SetState(unsigned long stateMask)
{
    VkState_SetState(stateMask);
}

void VkDrv_ResetState2D(void)
{
    VkState_Reset2D();
}

void VkDrv_ResetState3D(void)
{
    VkState_Reset3D();
}

void VkDrv_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane)
{
    VkState_SetPortalRendering(enabled, flipMatrix, plane);
}

void VkDrv_SetDepthRange(float minRange, float maxRange)
{
    VkState_SetDepthRange(minRange, maxRange);
}

void VkDrv_SetDrawBuffer(int buffer)
{
    // Vulkan handles draw buffers differently - this is mostly a no-op
    // The swapchain image is the draw buffer
}

//=============================================================================
// Matrices
//=============================================================================

void VkDrv_SetProjection(const float* projMatrix)
{
    VkState_SetProjection(projMatrix);
}

void VkDrv_GetProjection(float* projMatrix)
{
    VkState_GetProjection(projMatrix);
}

void VkDrv_SetModelView(const float* modelViewMatrix)
{
    VkState_SetModelView(modelViewMatrix);
}

void VkDrv_GetModelView(float* modelViewMatrix)
{
    VkState_GetModelView(modelViewMatrix);
}

//=============================================================================
// Viewport
//=============================================================================

void VkDrv_SetViewport(int left, int top, int width, int height)
{
    VkState_SetViewport(left, top, width, height);
}

//=============================================================================
// Clear
//=============================================================================

void VkDrv_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth)
{
    VkDraw_Clear(bits, clearCol, stencil, depth);
}

//=============================================================================
// Images
//=============================================================================

void VkDrv_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap)
{
    VkImage_Create(image, pic, isLightmap);
}

void VkDrv_DeleteImage(const image_t* image)
{
    VkImage_Delete(image);
}

void VkDrv_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty)
{
    VkImage_UpdateCinematic(image, pic, cols, rows, dirty);
}

imageFormat_t VkDrv_GetImageFormat(const image_t* image)
{
    return VkImage_GetFormat(image);
}

//=============================================================================
// Drawing
//=============================================================================

void VkDrv_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color)
{
    VkDraw_Image(image, coords, texcoords, color);
}

void VkDrv_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint)
{
    VkDraw_SkyBox(skybox, eye_origin, colorTint);
}

void VkDrv_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs)
{
    VkDraw_Beam(image, color, startPoints, endPoints, segs);
}

void VkDrv_DrawStageGeneric(const shaderCommands_t* input)
{
    VkDraw_StageGeneric(input);
}

void VkDrv_DrawStageVertexLitTexture(const shaderCommands_t* input)
{
    VkDraw_StageVertexLitTexture(input);
}

void VkDrv_DrawStageLightmappedMultitexture(const shaderCommands_t* input)
{
    VkDraw_StageLightmappedMultitexture(input);
}

void VkDrv_BeginTessellate(const shaderCommands_t* input)
{
    VkDraw_BeginTessellate(input);
}

void VkDrv_EndTessellate(const shaderCommands_t* input)
{
    VkDraw_EndTessellate(input);
}

//=============================================================================
// Shadows
//=============================================================================

void VkDrv_ShadowSilhouette(const float* edges, int edgeCount)
{
    VkDraw_ShadowSilhouette(edges, edgeCount);
}

void VkDrv_ShadowFinish(void)
{
    VkDraw_ShadowFinish();
}

//=============================================================================
// Debug
//=============================================================================

void VkDrv_DebugDrawAxis(void)
{
    // TODO: Implement debug axis drawing
}

void VkDrv_DebugDrawNormals(const shaderCommands_t* input)
{
    // TODO: Implement normal visualization
}

void VkDrv_DebugDrawTris(const shaderCommands_t* input)
{
    // TODO: Implement wireframe debug drawing
}

void VkDrv_DebugSetOverdrawMeasureEnabled(qboolean enabled)
{
    // TODO: Implement overdraw visualization
}

void VkDrv_DebugSetTextureMode(const char* mode)
{
    // TODO: Implement texture debug modes
}

void VkDrv_DebugDrawPolygon(int color, int numPoints, const float* points)
{
    // TODO: Implement debug polygon drawing
}
