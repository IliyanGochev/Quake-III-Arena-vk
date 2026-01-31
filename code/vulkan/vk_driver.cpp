#include "vk_common.h"
#include "vk_device.h"
#include "vk_driver.h"
#include "vk_state.h"
#include "vk_draw.h"
#include "vk_image.h"
#include "vk_shaders.h"
#include "../win32/win_vk.h"

// External state from vk_draw.cpp
extern vkDrawState_t g_vkDraw;

//----------------------------------------------------------------------------
// Vulkan CVars
//----------------------------------------------------------------------------
cvar_t* r_driver = nullptr;
cvar_t* vk_validation = nullptr;
cvar_t* vk_multisamples = nullptr;
cvar_t* vk_anisotropy = nullptr;
cvar_t* vk_vsync = nullptr;

//----------------------------------------------------------------------------
// Setup video configuration
//----------------------------------------------------------------------------
void VK_SetupVideoConfig(void) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(g_vkDevice.physicalDevice, &deviceProperties);

    Q_strncpyz(vdConfig.renderer_string, "Vulkan", sizeof(vdConfig.renderer_string));
    Q_strncpyz(vdConfig.version_string, va("v%d.%d.%d",
        VK_VERSION_MAJOR(deviceProperties.apiVersion),
        VK_VERSION_MINOR(deviceProperties.apiVersion),
        VK_VERSION_PATCH(deviceProperties.apiVersion)), sizeof(vdConfig.version_string));
    Q_strncpyz(vdConfig.vendor_string, deviceProperties.deviceName, sizeof(vdConfig.vendor_string));
    vdConfig.maxTextureSize = deviceProperties.limits.maxImageDimension2D;
    vdConfig.colorBits = 32;
    vdConfig.depthBits = 24;
    vdConfig.stencilBits = 8;
    vdConfig.deviceSupportsGamma = qfalse;  // Handle via OS/driver
    vdConfig.displayFrequency = 60;

    cvar_t* r_fullscreen = ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
    vdConfig.isFullscreen = r_fullscreen->integer ? qtrue : qfalse;

    vdConfig.vidWidth = g_vkDevice.swapchainExtent.width;
    vdConfig.vidHeight = g_vkDevice.swapchainExtent.height;
    vdConfig.windowAspect = (float)vdConfig.vidWidth / (float)vdConfig.vidHeight;

    ri.Printf(PRINT_ALL, "...VK_SetupVideoConfig: swapchainExtent=%dx%d, vdConfig=%dx%d\n",
              g_vkDevice.swapchainExtent.width, g_vkDevice.swapchainExtent.height,
              vdConfig.vidWidth, vdConfig.vidHeight);
}

//----------------------------------------------------------------------------
// Driver initialization
//----------------------------------------------------------------------------
void VK_DriverInit(void) {
    ri.Printf(PRINT_ALL, "------- VK_DriverInit -------\n");

    // Register CVars
    r_driver = ri.Cvar_Get("r_driver", "opengl", CVAR_ARCHIVE | CVAR_LATCH);
#ifdef _DEBUG
    vk_validation = ri.Cvar_Get("vk_validation", "1", CVAR_ARCHIVE);
#else
    vk_validation = ri.Cvar_Get("vk_validation", "0", CVAR_ARCHIVE);
#endif
    vk_multisamples = ri.Cvar_Get("vk_multisamples", "1", CVAR_ARCHIVE | CVAR_LATCH);
    vk_anisotropy = ri.Cvar_Get("vk_anisotropy", "16", CVAR_ARCHIVE);
    vk_vsync = ri.Cvar_Get("vk_vsync", "1", CVAR_ARCHIVE | CVAR_LATCH);

    ri.Printf(PRINT_ALL, "  Validation: %s\n", vk_validation->integer ? "Enabled" : "Disabled");
    ri.Printf(PRINT_ALL, "  MSAA: %dx\n", vk_multisamples->integer);
    ri.Printf(PRINT_ALL, "  Anisotropy: %dx\n", vk_anisotropy->integer);
    ri.Printf(PRINT_ALL, "  VSync: %s\n", vk_vsync->integer ? "Enabled" : "Disabled");

    // Assign function pointers
    GFX_Shutdown = VK_Shutdown;
    GFX_UnbindResources = VK_UnbindResources;
    GFX_LastError = VK_LastError;
    GFX_ReadPixels = VK_ReadPixels;
    GFX_ReadDepth = VK_ReadDepth;
    GFX_ReadStencil = VK_ReadStencil;
    GFX_CreateImage = VK_CreateImage;
    GFX_DeleteImage = VK_DeleteImage;
    GFX_UpdateCinematic = VK_UpdateCinematic;
    GFX_DrawImage = VK_DrawImage;
    GFX_GetImageFormat = VK_GetImageFormat;
    GFX_SetGamma = VK_SetGamma;
    GFX_GetFrameImageMemoryUsage = VK_GetFrameImageMemoryUsage;
    GFX_GraphicsInfo = VK_GraphicsInfo;
    GFX_Clear = VK_Clear;
    GFX_SetProjectionMatrix = VK_SetProjectionMatrix;
    GFX_GetProjectionMatrix = VK_GetProjectionMatrix;
    GFX_SetModelViewMatrix = VK_SetModelViewMatrix;
    GFX_GetModelViewMatrix = VK_GetModelViewMatrix;
    GFX_SetViewport = VK_SetViewport;
    GFX_Flush = VK_Flush;
    GFX_SetState = VK_SetState;
    GFX_ResetState2D = VK_ResetState2D;
    GFX_ResetState3D = VK_ResetState3D;
    GFX_SetPortalRendering = VK_SetPortalRendering;
    GFX_SetDepthRange = VK_SetDepthRange;
    GFX_SetDrawBuffer = VK_SetDrawBuffer;
    GFX_EndFrame = VK_EndFrame;
    GFX_MakeCurrent = VK_MakeCurrent;
    GFX_ShadowSilhouette = VK_ShadowSilhouette;
    GFX_ShadowFinish = VK_ShadowFinish;
    GFX_DrawSkyBox = VK_DrawSkyBox;
    GFX_DrawBeam = VK_DrawBeam;
    GFX_DrawStageGeneric = VK_DrawStageGeneric;
    GFX_DrawStageVertexLitTexture = VK_DrawStageVertexLitTexture;
    GFX_DrawStageLightmappedMultitexture = VK_DrawStageLightmappedMultitexture;
    GFX_BeginTessellate = VK_BeginTessellate;
    GFX_EndTessellate = VK_EndTessellate;
    GFX_DebugDrawAxis = VK_DebugDrawAxis;
    GFX_DebugDrawNormals = VK_DebugDrawNormals;
    GFX_DebugDrawTris = VK_DebugDrawTris;
    GFX_DebugSetOverdrawMeasureEnabled = VK_DebugSetOverdrawMeasureEnabled;
    GFX_DebugSetTextureMode = VK_DebugSetTextureMode;
    GFX_DebugDrawPolygon = VK_DebugDrawPolygon;

    // Initialize window and Vulkan device
    VKWnd_Init();

    // Initialize image system
    VK_InitImages();

    // Initialize pipeline state
    VK_InitPipelineState();

    // Create frame resources
    VK_CreateFrameResources();

    // Setup video config
    VK_SetupVideoConfig();

    // Initialize runtime state
    memset(&g_vkRunState, 0, sizeof(g_vkRunState));

    // Initialize matrices to identity (matches D3D11 lines 370-371)
    memcpy(g_vkRunState.modelViewMatrix, s_identityMatrix, sizeof(float) * 16);
    memcpy(g_vkRunState.projectionMatrix, s_identityMatrix, sizeof(float) * 16);

    // Initialize depth range (matches D3D11 lines 372-373)
    g_vkRunState.depthRange[0] = 0.0f;
    g_vkRunState.depthRange[1] = 1.0f;

    g_vkRunState.viewVSDirty = qtrue;
    g_vkRunState.viewPSDirty = qtrue;

    // Initialize alpha clip to default "no alpha test" state (matches D3D11)
    g_vkRunState.alphaClip[0] = 1.0f;
    g_vkRunState.alphaClip[1] = 0.0f;

    // Initialize cull mode to -1 to force first state commit (matches D3D11 line 377)
    g_vkRunState.cullMode = -1;

    ri.Printf(PRINT_ALL, "------- VK_DriverInit Complete -------\n");
}

//----------------------------------------------------------------------------
// Shutdown
//----------------------------------------------------------------------------
void VK_Shutdown(void) {
    ri.Printf(PRINT_ALL, "------- VK_Shutdown -------\n");

    // Wait for device idle
    VK_FlushGPU();

    // Destroy frame resources
    VK_DestroyFrameResources();

    // Destroy pipeline state
    VK_DestroyPipelineState();

    // Shutdown image system
    VK_ShutdownImages();

    // Shutdown window
    VKWnd_Shutdown();

    ri.Printf(PRINT_ALL, "------- VK_Shutdown Complete -------\n");
}

//----------------------------------------------------------------------------
// Unbind resources (not needed for Vulkan)
//----------------------------------------------------------------------------
void VK_UnbindResources(void) {
    // No-op for Vulkan
}

//----------------------------------------------------------------------------
// Last error
//----------------------------------------------------------------------------
size_t VK_LastError(void) {
    return 0;  // Error handling done via VK_CHECK macro
}

//----------------------------------------------------------------------------
// Read pixels (readback from GPU)
//----------------------------------------------------------------------------
void VK_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFmt, void* dest) {
    // TODO: Implement GPU readback
    ri.Printf(PRINT_WARNING, "WARNING: VK_ReadPixels not yet implemented\n");
}

//----------------------------------------------------------------------------
// Read depth (readback from GPU)
//----------------------------------------------------------------------------
void VK_ReadDepth(int x, int y, int width, int height, float* dest) {
    // TODO: Implement depth readback
    ri.Printf(PRINT_WARNING, "WARNING: VK_ReadDepth not yet implemented\n");
}

//----------------------------------------------------------------------------
// Read stencil (readback from GPU)
//----------------------------------------------------------------------------
void VK_ReadStencil(int x, int y, int width, int height, byte* dest) {
    // TODO: Implement stencil readback
    ri.Printf(PRINT_WARNING, "WARNING: VK_ReadStencil not yet implemented\n");
}

//----------------------------------------------------------------------------
// Create image
//----------------------------------------------------------------------------
void VK_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap) {
    VK_CreateImageFromPixels(image, pic, isLightmap);
}

//----------------------------------------------------------------------------
// Delete image
//----------------------------------------------------------------------------
void VK_DeleteImage(const image_t* image) {
    // Ensure GPU is not using the image before deleting
    // NOTE: Do NOT call VK_EndFrame() here as it would end the render pass mid-frame,
    // causing the next draw to clear the framebuffer. Just flush the GPU instead.
    VK_FlushGPU();
    VK_DeleteImageInternal(image);
}

//----------------------------------------------------------------------------
// Update cinematic
//----------------------------------------------------------------------------
void VK_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty) {
    VK_UpdateDynamicImage(image, pic, cols, rows, dirty);
}

//----------------------------------------------------------------------------
// Draw image (2D/UI)
//----------------------------------------------------------------------------
void VK_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color) {
    if (!image) {
        return;
    }

    vkImage_t* vkImg = VK_GetImage(image);
    if (!vkImg || vkImg->image == VK_NULL_HANDLE) {
        return;
    }

    // DEBUG: Log first few DrawImage calls to help diagnose double rendering
    static int drawImageCallCount = 0;
    if (drawImageCallCount < 10) {
        ri.Printf(PRINT_ALL, "VK_DrawImage #%d: image='%s', coords=(%.1f,%.1f,%.1f,%.1f), inRenderPass=%d\n",
                  drawImageCallCount, image->imgName,
                  coords[0], coords[1], coords[2], coords[3],
                  g_vkDraw.inRenderPass);
        drawImageCallCount++;
    }

    // Lazy frame begin - start frame if not already started
    if (!g_vkDraw.inRenderPass) {
        VK_BeginFrame();

        // CRITICAL: Re-apply the viewport after BeginFrame
        // VK_BeginFrame sets viewport to swapchain size, but we need the
        // viewport that was set by Set2DProjection to match the orthographic projection
        if (g_vkRunState.viewportWidth > 0 && g_vkRunState.viewportHeight > 0) {
            VK_SetViewport(g_vkRunState.viewportX, g_vkRunState.viewportY,
                          g_vkRunState.viewportWidth, g_vkRunState.viewportHeight);
        } else {
            // Fallback: viewport not set, use full screen
            // This shouldn't happen but prevents undefined behavior
            VK_SetViewport(0, 0, vdConfig.vidWidth, vdConfig.vidHeight);
        }
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Update view uniforms if dirty
    if (g_vkRunState.viewVSDirty) {
        VK_UpdateViewVSUniform();
    }
    if (g_vkRunState.viewPSDirty) {
        VK_UpdateViewPSUniform();
    }

    // Update view descriptor set
    VK_UpdateViewDescriptorSet(g_vkDraw.currentFrame);

    // Set up vertex data (4 vertices for a quad, 2 triangles)
    // FSQ shader expects TWO separate buffers: positions and texcoords
    // IMPORTANT: Match D3D11 vertex order exactly (d3d_draw.cpp:233-241)
    float positions[4][2];
    float texCoordsData[4][2];

    // Vertex 0: Top-left (coords[0], coords[1])
    positions[0][0] = coords[0];
    positions[0][1] = coords[1];
    texCoordsData[0][0] = texcoords[0];
    texCoordsData[0][1] = texcoords[1];

    // Vertex 1: Top-right (coords[2], coords[1])
    positions[1][0] = coords[2];
    positions[1][1] = coords[1];
    texCoordsData[1][0] = texcoords[2];
    texCoordsData[1][1] = texcoords[1];

    // Vertex 2: Bottom-right (coords[2], coords[3])
    positions[2][0] = coords[2];
    positions[2][1] = coords[3];
    texCoordsData[2][0] = texcoords[2];
    texCoordsData[2][1] = texcoords[3];

    // Vertex 3: Bottom-left (coords[0], coords[3])
    positions[3][0] = coords[0];
    positions[3][1] = coords[3];
    texCoordsData[3][0] = texcoords[0];
    texCoordsData[3][1] = texcoords[3];

    // Upload position data to circular buffer
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.xyz, positions, sizeof(positions));
    uint32_t positionOffset = g_vkDraw.tessBuffers.xyz.currentOffset;

    // Upload texcoord data to circular buffer (using stage 0's first texcoord buffer)
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.stages[0].texCoords[0], texCoordsData, sizeof(texCoordsData));
    uint32_t texCoordOffset = g_vkDraw.tessBuffers.stages[0].texCoords[0].currentOffset;

    // Index data (2 triangles: 0-1-2, 0-2-3)
    uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.indexes, indices, sizeof(indices));
    uint32_t indexOffset = g_vkDraw.tessBuffers.indexes.currentOffset;

    // Update stage uniform (for color modulation)
    vkStageUniform_t stageUniform;
    if (color) {
        stageUniform.color[0] = color[0];
        stageUniform.color[1] = color[1];
        stageUniform.color[2] = color[2];
        stageUniform.color[3] = 1.0f;
    } else {
        stageUniform.color[0] = 1.0f;
        stageUniform.color[1] = 1.0f;
        stageUniform.color[2] = 1.0f;
        stageUniform.color[3] = 1.0f;
    }
    VK_UpdateCircularBuffer(&g_vkDraw.uniformBuffers.stage, &stageUniform, sizeof(stageUniform));

    // Build pipeline key for FSQ shader (fullscreen quad / 2D)
    vkPipelineKey_t key = {};
    key.shaderType = VK_SHADER_FSQ;
    key.blendSrc = GLS_SRCBLEND_SRC_ALPHA;
    key.blendDst = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    key.depthFlags = GLS_DEPTHTEST_DISABLE;  // 2D rendering, no depth test
    key.cullMode = CT_TWO_SIDED;
    key.polygonMode = 0;  // Fill mode
    key.sampleCount = g_vkDevice.msaaSamples;

    // Get or create pipeline
    VkPipeline pipeline = VK_GetOrCreatePipeline(key);
    if (pipeline == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "WARNING: Failed to get FSQ pipeline\n");
        return;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind index buffer
    vkCmdBindIndexBuffer(cmd, g_vkDraw.tessBuffers.indexes.buffer, indexOffset, VK_INDEX_TYPE_UINT16);

    // Bind vertex buffers (positions and texcoords as separate buffers)
    VkBuffer vertexBuffers[] = {
        g_vkDraw.tessBuffers.xyz.buffer,                           // Binding 0: positions
        g_vkDraw.tessBuffers.stages[0].texCoords[0].buffer         // Binding 1: texcoords
    };
    VkDeviceSize offsets[] = {positionOffset, texCoordOffset};
    vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);

    // Update stage descriptor set
    VK_UpdateStageDescriptorSet(g_vkDraw.currentFrame);

    // Bind descriptor set 0 (view uniforms)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 0, 1,
                           &g_vkDraw.descriptorSets.currentViewSet, 0, nullptr);

    // Bind descriptor set 1 (stage uniforms)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 1, 1,
                           &g_vkDraw.descriptorSets.currentStageSet, 0, nullptr);

    // Bind descriptor set 2 (texture)
    if (vkImg->descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               g_vkPipelines.pipelineLayout, 2, 1,
                               &vkImg->descriptorSet, 0, nullptr);
    } else {
        ri.Printf(PRINT_WARNING, "WARNING: VK_DrawImage - Texture descriptor set is NULL for '%s', skipping draw\n",
                  image ? image->imgName : "NULL");
        return;  // Don't draw with stale descriptor set
    }

    // Draw the quad
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

//----------------------------------------------------------------------------
// Get image format
//----------------------------------------------------------------------------
imageFormat_t VK_GetImageFormat(const image_t* image) {
    return VK_GetImageFormatInternal(image);
}

//----------------------------------------------------------------------------
// Set gamma (handled by OS/driver)
//----------------------------------------------------------------------------
void VK_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]) {
    // No-op - gamma handled by OS/driver
}

//----------------------------------------------------------------------------
// Get frame image memory usage
//----------------------------------------------------------------------------
int VK_GetFrameImageMemoryUsage(void) {
    // TODO: Query VMA for memory usage
    return 0;
}

//----------------------------------------------------------------------------
// Graphics info
//----------------------------------------------------------------------------
void VK_GraphicsInfo(void) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(g_vkDevice.physicalDevice, &deviceProperties);

    ri.Printf(PRINT_ALL, "\nVulkan Graphics Info:\n");
    ri.Printf(PRINT_ALL, "  Device: %s\n", deviceProperties.deviceName);
    ri.Printf(PRINT_ALL, "  API Version: %d.%d.%d\n",
        VK_VERSION_MAJOR(deviceProperties.apiVersion),
        VK_VERSION_MINOR(deviceProperties.apiVersion),
        VK_VERSION_PATCH(deviceProperties.apiVersion));
    ri.Printf(PRINT_ALL, "  Driver Version: %d.%d.%d\n",
        VK_VERSION_MAJOR(deviceProperties.driverVersion),
        VK_VERSION_MINOR(deviceProperties.driverVersion),
        VK_VERSION_PATCH(deviceProperties.driverVersion));
    ri.Printf(PRINT_ALL, "  Vendor ID: 0x%04X\n", deviceProperties.vendorID);
    ri.Printf(PRINT_ALL, "  Device ID: 0x%04X\n", deviceProperties.deviceID);
    ri.Printf(PRINT_ALL, "  Max Texture Size: %d\n", deviceProperties.limits.maxImageDimension2D);
    ri.Printf(PRINT_ALL, "  MSAA Samples: %dx\n", (int)g_vkDevice.msaaSamples);
    ri.Printf(PRINT_ALL, "\n");
}

//----------------------------------------------------------------------------
// Clear
//----------------------------------------------------------------------------
void VK_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth) {
    // Clearing is done at render pass begin in VK_BeginFrame
    // This function is mostly a no-op in Vulkan
}

//----------------------------------------------------------------------------
// Set projection matrix
//----------------------------------------------------------------------------
void VK_SetProjectionMatrix(const float* projMatrix) {
    // Copy the matrix
    memcpy(g_vkRunState.projectionMatrix, projMatrix, sizeof(float) * 16);

    // Convert OpenGL depth range [-1, 1] to Vulkan depth range [0, 1]
    // This formula works for BOTH orthographic and perspective projections
    g_vkRunState.projectionMatrix[10] = projMatrix[10] * 0.5f - projMatrix[11] * 0.5f;
    g_vkRunState.projectionMatrix[14] = projMatrix[14] * 0.5f + 0.5f;

    g_vkRunState.viewVSDirty = qtrue;
}

//----------------------------------------------------------------------------
// Get projection matrix
//----------------------------------------------------------------------------
void VK_GetProjectionMatrix(float* projMatrix) {
    memcpy(projMatrix, g_vkRunState.projectionMatrix, sizeof(float) * 16);
}

//----------------------------------------------------------------------------
// Set model-view matrix
//----------------------------------------------------------------------------
void VK_SetModelViewMatrix(const float* modelViewMatrix) {
    memcpy(g_vkRunState.modelViewMatrix, modelViewMatrix, sizeof(float) * 16);
    g_vkRunState.viewVSDirty = qtrue;
}

//----------------------------------------------------------------------------
// Get model-view matrix
//----------------------------------------------------------------------------
void VK_GetModelViewMatrix(float* modelViewMatrix) {
    memcpy(modelViewMatrix, g_vkRunState.modelViewMatrix, sizeof(float) * 16);
}

//----------------------------------------------------------------------------
// Set viewport
//----------------------------------------------------------------------------
void VK_SetViewport(int left, int top, int width, int height) {
    static int logCount = 0;
    if (logCount < 5) {  // Only log first 5 calls to avoid spam
        ri.Printf(PRINT_ALL, "VK_SetViewport: left=%d, top=%d, width=%d, height=%d (swapchainExtent=%dx%d)\n",
                  left, top, width, height,
                  g_vkDevice.swapchainExtent.width, g_vkDevice.swapchainExtent.height);
        logCount++;
    }

    // Only set viewport/scissor if we're in a render pass
    // Otherwise just store the values for when the render pass begins
    if (!g_vkDraw.inRenderPass) {
        g_vkRunState.viewportX = left;
        g_vkRunState.viewportY = top;
        g_vkRunState.viewportWidth = width;
        g_vkRunState.viewportHeight = height;
        return;
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Convert OpenGL window coordinates to Vulkan framebuffer coordinates
    // OpenGL: y=0 at bottom, increases upward
    // Vulkan: y=0 at top, increases downward
    // With negative height, viewport.y specifies the BOTTOM edge
    int screenHeight = g_vkDevice.swapchainExtent.height;

    VkViewport viewport = {};
    viewport.x = (float)left;
    viewport.y = (float)(screenHeight - top);  // Bottom edge in Vulkan coords
    viewport.width = (float)width;
    viewport.height = -(float)height;  // Negative to flip Y axis
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    if (logCount < 5) {
        ri.Printf(PRINT_ALL, "  -> Vulkan viewport: x=%.0f, y=%.0f, width=%.0f, height=%.0f\n",
                  viewport.x, viewport.y, viewport.width, viewport.height);
    }

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Like D3D11, we set scissor to full screen (effectively disabling it)
    // This avoids any scissor clipping issues
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_vkDevice.swapchainExtent;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    g_vkRunState.viewportX = left;
    g_vkRunState.viewportY = top;
    g_vkRunState.viewportWidth = width;
    g_vkRunState.viewportHeight = height;
}

//----------------------------------------------------------------------------
// Flush
//----------------------------------------------------------------------------
void VK_Flush(void) {
    // NOTE: In Vulkan, Flush is a no-op
    // The frame-in-flight system already handles GPU synchronization
    // The frame will be ended by VK_EndFrame which is called from RB_SwapBuffers
    // Calling vkDeviceWaitIdle here would stall the pipeline unnecessarily
}

//----------------------------------------------------------------------------
// Set state
//----------------------------------------------------------------------------
void VK_SetState(unsigned long stateMask) {
    // Calculate what changed
    unsigned long diff = stateMask ^ g_vkRunState.stateMask;

    // Store state
    g_vkRunState.stateMask = stateMask;

    // Handle alpha test state changes (matches D3D11 d3d_state.cpp lines 177-204)
    // Alpha test is emulated in fragment shaders via alphaClip uniform
    if (diff & GLS_ATEST_BITS) {
        const float alphaEps = 0.00001f;
        switch (stateMask & GLS_ATEST_BITS) {
            case 0:
                g_vkRunState.alphaClip[0] = 1;
                g_vkRunState.alphaClip[1] = 0;
                break;
            case GLS_ATEST_GT_0:
                g_vkRunState.alphaClip[0] = 1;
                g_vkRunState.alphaClip[1] = alphaEps;
                break;
            case GLS_ATEST_LT_80:
                g_vkRunState.alphaClip[0] = -1;
                g_vkRunState.alphaClip[1] = 0.5f;
                break;
            case GLS_ATEST_GE_80:
                g_vkRunState.alphaClip[0] = 1;
                g_vkRunState.alphaClip[1] = 0.5f;
                break;
        }
        g_vkRunState.viewPSDirty = qtrue;
    }

    // Pipeline binding happens in drawing functions based on shader type + state
}

//----------------------------------------------------------------------------
// Reset state for 2D rendering
//----------------------------------------------------------------------------
void VK_ResetState2D(void) {
    g_vkRunState.stateMask = GLS_DEPTHTEST_DISABLE |
                             GLS_SRCBLEND_SRC_ALPHA |
                             GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

    // Set model-view matrix to identity for 2D rendering (same as D3D11)
    memcpy(g_vkRunState.modelViewMatrix, s_identityMatrix, sizeof(float) * 16);
    g_vkRunState.viewVSDirty = qtrue;

    // Reset portal rendering (clip plane) - matches D3D11 line 164
    VK_SetPortalRendering(qfalse, NULL, NULL);

    // Reset depth range for 2D rendering (matches D3D11 line 165)
    VK_SetDepthRange(0, 0);
}

//----------------------------------------------------------------------------
// Reset state for 3D rendering
//----------------------------------------------------------------------------
void VK_ResetState3D(void) {
    // Reset model-view matrix to identity (matches D3D11 line 170)
    memcpy(g_vkRunState.modelViewMatrix, s_identityMatrix, sizeof(float) * 16);
    g_vkRunState.viewVSDirty = qtrue;

    // Set default state: depth mask true, depth test ENABLED (matches D3D11 line 171: GLS_DEFAULT)
    // Note: GLS_DEFAULT = GLS_DEPTHMASK_TRUE (defined in tr_state.h)
    // Depth test is ENABLED when GLS_DEPTHTEST_DISABLE is NOT set
    g_vkRunState.stateMask = GLS_DEFAULT;

    // Reset depth range for 3D rendering (matches D3D11 line 172)
    VK_SetDepthRange(0, 1);
}

//----------------------------------------------------------------------------
// Set portal rendering
//----------------------------------------------------------------------------
void VK_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane) {
    if (enabled && plane) {
        g_vkRunState.clipPlane[0] = plane[0];
        g_vkRunState.clipPlane[1] = plane[1];
        g_vkRunState.clipPlane[2] = plane[2];
        g_vkRunState.clipPlane[3] = plane[3];
    } else {
        g_vkRunState.clipPlane[0] = 0.0f;
        g_vkRunState.clipPlane[1] = 0.0f;
        g_vkRunState.clipPlane[2] = 0.0f;
        g_vkRunState.clipPlane[3] = 0.0f;
    }
    g_vkRunState.viewPSDirty = qtrue;
}

//----------------------------------------------------------------------------
// Set depth range
//----------------------------------------------------------------------------
void VK_SetDepthRange(float minRange, float maxRange) {
    g_vkRunState.depthRange[0] = minRange;
    g_vkRunState.depthRange[1] = maxRange;
    g_vkRunState.viewVSDirty = qtrue;
}

//----------------------------------------------------------------------------
// Set draw buffer (not applicable for Vulkan)
//----------------------------------------------------------------------------
void VK_SetDrawBuffer(int buffer) {
    // No-op for Vulkan
}

//----------------------------------------------------------------------------
// Make current (not applicable for Vulkan)
//----------------------------------------------------------------------------
void VK_MakeCurrent(qboolean current) {
    // No-op for Vulkan
}

//----------------------------------------------------------------------------
// Shadow silhouette
//----------------------------------------------------------------------------
void VK_ShadowSilhouette(const float* edges, int edgeCount) {
    // TODO: Implement stencil shadow volumes
    ri.Printf(PRINT_DEVELOPER, "VK_ShadowSilhouette not yet implemented\n");
}

//----------------------------------------------------------------------------
// Shadow finish
//----------------------------------------------------------------------------
void VK_ShadowFinish(void) {
    // TODO: Implement stencil shadow volumes
}

//----------------------------------------------------------------------------
// Draw skybox
//----------------------------------------------------------------------------
void VK_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint) {
    if (!skybox) {
        return;
    }

    // Lazy frame begin - start frame if not already started
    if (!g_vkDraw.inRenderPass) {
        VK_BeginFrame();
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Skybox vertex data (interleaved position + texcoord)
    // Same layout as D3D11: position (3 floats) + texcoord (2 floats) = 5 floats per vertex
    static const float skyboxVertexData[] = {
        // Right (+X)
         1, -1, -1, 1, 1,  // vertex 0
         1, -1,  1, 1, 0,  // vertex 1
         1,  1,  1, 0, 0,  // vertex 2
         1, -1, -1, 1, 1,  // vertex 3
         1,  1,  1, 0, 0,  // vertex 4
         1,  1, -1, 0, 1,  // vertex 5

        // Left (-X)
        -1, -1,  1, 0, 0,  // vertex 6
        -1, -1, -1, 0, 1,  // vertex 7
        -1,  1, -1, 1, 1,  // vertex 8
        -1, -1,  1, 0, 0,  // vertex 9
        -1,  1, -1, 1, 1,  // vertex 10
        -1,  1,  1, 1, 0,  // vertex 11

        // Back (+Y)
        -1,  1,  1, 0, 0,  // vertex 12
        -1,  1, -1, 0, 1,  // vertex 13
         1,  1, -1, 1, 1,  // vertex 14
        -1,  1,  1, 0, 0,  // vertex 15
         1,  1, -1, 1, 1,  // vertex 16
         1,  1,  1, 1, 0,  // vertex 17

        // Front (-Y)
         1, -1,  1, 0, 0,  // vertex 18
         1, -1, -1, 0, 1,  // vertex 19
        -1, -1, -1, 1, 1,  // vertex 20
         1, -1,  1, 0, 0,  // vertex 21
        -1, -1, -1, 1, 1,  // vertex 22
        -1, -1,  1, 1, 0,  // vertex 23

        // Up (+Z)
         1, -1,  1, 1, 1,  // vertex 24
        -1, -1,  1, 1, 0,  // vertex 25
        -1,  1,  1, 0, 0,  // vertex 26
         1, -1,  1, 1, 1,  // vertex 27
        -1,  1,  1, 0, 0,  // vertex 28
         1,  1,  1, 0, 1,  // vertex 29

        // Down (-Z)
        -1, -1, -1, 1, 0,  // vertex 30
         1, -1, -1, 1, 1,  // vertex 31
         1,  1, -1, 0, 1,  // vertex 32
        -1, -1, -1, 1, 0,  // vertex 33
         1,  1, -1, 0, 1,  // vertex 34
        -1,  1, -1, 0, 0,  // vertex 35
    };

    // Upload vertex data to circular buffer
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.xyz, skyboxVertexData, sizeof(skyboxVertexData));
    uint32_t vertexOffset = g_vkDraw.tessBuffers.xyz.currentOffset;

    // Update view uniforms if dirty
    if (g_vkRunState.viewVSDirty) {
        VK_UpdateViewVSUniform();
    }
    if (g_vkRunState.viewPSDirty) {
        VK_UpdateViewPSUniform();
    }

    // Update view descriptor set
    VK_UpdateViewDescriptorSet(g_vkDraw.currentFrame);

    // Update stage uniform (for color tint)
    vkStageUniform_t stageUniform;
    if (colorTint) {
        stageUniform.color[0] = colorTint[0];
        stageUniform.color[1] = colorTint[1];
        stageUniform.color[2] = colorTint[2];
        stageUniform.color[3] = 1.0f;
    } else {
        stageUniform.color[0] = 1.0f;
        stageUniform.color[1] = 1.0f;
        stageUniform.color[2] = 1.0f;
        stageUniform.color[3] = 1.0f;
    }
    VK_UpdateCircularBuffer(&g_vkDraw.uniformBuffers.stage, &stageUniform, sizeof(stageUniform));

    // Update stage descriptor set
    VK_UpdateStageDescriptorSet(g_vkDraw.currentFrame);

    // Set state (no blending, two-sided rendering)
    VK_SetState(0);

    // Build pipeline key for skybox shader
    vkPipelineKey_t key = {};
    key.shaderType = VK_SHADER_SKYBOX;
    key.blendSrc = GLS_SRCBLEND_ONE;
    key.blendDst = GLS_DSTBLEND_ZERO;
    key.depthFlags = 0;  // Normal depth test
    key.cullMode = CT_TWO_SIDED;
    key.polygonMode = 0;  // Fill mode
    key.sampleCount = g_vkDevice.msaaSamples;

    // Get or create pipeline
    VkPipeline pipeline = VK_GetOrCreatePipeline(key);
    if (pipeline == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "WARNING: Failed to get skybox pipeline\n");
        return;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex buffer (single interleaved buffer)
    VkBuffer vertexBuffers[] = {g_vkDraw.tessBuffers.xyz.buffer};
    VkDeviceSize offsets[] = {vertexOffset};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    // Bind descriptor set 0 (view uniforms)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 0, 1,
                           &g_vkDraw.descriptorSets.currentViewSet, 0, nullptr);

    // Bind descriptor set 1 (stage uniforms)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 1, 1,
                           &g_vkDraw.descriptorSets.currentStageSet, 0, nullptr);

    // Draw each side of the skybox (6 sides, 6 vertices each)
    for (int i = 0; i < 6; ++i) {
        const skyboxSideDrawInfo_t* side = &skybox->sides[i];

        if (!side->image) {
            continue;
        }

        // Get Vulkan image
        vkImage_t* vkImg = VK_GetImage(side->image);
        if (!vkImg || vkImg->image == VK_NULL_HANDLE || vkImg->descriptorSet == VK_NULL_HANDLE) {
            continue;
        }

        // Bind descriptor set 2 (texture)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               g_vkPipelines.pipelineLayout, 2, 1,
                               &vkImg->descriptorSet, 0, nullptr);

        // Push constant for eye position offset
        if (eye_origin) {
            vkCmdPushConstants(cmd, g_vkPipelines.pipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 3, eye_origin);
        } else {
            float zero[3] = {0.0f, 0.0f, 0.0f};
            vkCmdPushConstants(cmd, g_vkPipelines.pipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 3, zero);
        }

        // Draw this side (6 vertices starting at i * 6)
        vkCmdDraw(cmd, 6, 1, i * 6, 0);
    }
}

//----------------------------------------------------------------------------
// Draw beam
//----------------------------------------------------------------------------
void VK_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs) {
    // Not implemented - after a grep of the BSP files there is no reference to RT_BEAM anywhere.
    // This matches the D3D11 implementation which also skips beam rendering.
}

//----------------------------------------------------------------------------
// Draw stage generic (main rendering function)
//----------------------------------------------------------------------------
void VK_DrawStageGeneric(const shaderCommands_t* input) {
    if (!input || input->numIndexes == 0 || input->numVertexes == 0) {
        return;
    }

    // Lazy frame begin - start frame if not already started
    if (!g_vkDraw.inRenderPass) {
        VK_BeginFrame();
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Update view uniforms if dirty
    if (g_vkRunState.viewVSDirty) {
        VK_UpdateViewVSUniform();
    }
    if (g_vkRunState.viewPSDirty) {
        VK_UpdateViewPSUniform();
    }

    // Update view descriptor set
    VK_UpdateViewDescriptorSet(g_vkDraw.currentFrame);

    // Upload tessellation data
    VK_UpdateTessBuffers(input, qfalse, qfalse);

    // Bind index buffer
    VkBuffer indexBuffer = g_vkDraw.tessBuffers.indexes.buffer;
    VkDeviceSize indexOffset = g_vkDraw.tessBuffers.indexes.currentOffset;
    vkCmdBindIndexBuffer(cmd, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT16);

    // Bind position buffer (binding 0)
    VkBuffer positionBuffer = g_vkDraw.tessBuffers.xyz.buffer;
    VkDeviceSize positionOffset = g_vkDraw.tessBuffers.xyz.currentOffset;
    vkCmdBindVertexBuffers(cmd, 0, 1, &positionBuffer, &positionOffset);

    // Bind view descriptor set (set 0)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 0, 1,
                           &g_vkDraw.descriptorSets.currentViewSet, 0, nullptr);

    // Draw each stage
    for (int stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        const shaderStage_t* pStage = input->xstages[stage];
        if (!pStage) break;

        // Determine shader type
        vkShaderType_t shaderType;
        if (pStage->bundle[1].image[0]) {
            shaderType = VK_SHADER_MULTI_TEXTURE;
        } else {
            shaderType = VK_SHADER_SINGLE_TEXTURE;
        }

        // Build pipeline key
        vkPipelineKey_t key = {};
        key.shaderType = shaderType;
        key.blendSrc = pStage->stateBits & GLS_SRCBLEND_BITS;
        key.blendDst = pStage->stateBits & GLS_DSTBLEND_BITS;

        // For 2D rendering, use global state (from VK_ResetState2D) for depth flags
        // For 3D rendering, use per-stage state
        if (backEnd.projection2D) {
            key.depthFlags = g_vkRunState.stateMask & (GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE | GLS_DEPTHFUNC_EQUAL);
        } else {
            key.depthFlags = pStage->stateBits & (GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE | GLS_DEPTHFUNC_EQUAL);
        }

        key.cullMode = input->shader->cullType;
        key.polygonMode = 0;  // Fill mode
        key.sampleCount = g_vkDevice.msaaSamples;

        // Get or create pipeline
        VkPipeline pipeline = VK_GetOrCreatePipeline(key);
        if (pipeline == VK_NULL_HANDLE) {
            ri.Printf(PRINT_WARNING, "WARNING: Failed to get pipeline\n");
            continue;
        }

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind vertex buffers for this stage
        if (shaderType == VK_SHADER_MULTI_TEXTURE) {
            VK_SetVertexBuffersMT(&g_vkDraw.tessBuffers.stages[stage]);
        } else {
            VK_SetVertexBuffersST(&g_vkDraw.tessBuffers.stages[stage]);
        }

        // Bind texture descriptor set (set 2)
        image_t* texture = pStage->bundle[0].image[0];
        if (!texture) {
            continue;  // Skip stage without texture to avoid using stale descriptor set
        }

        vkImage_t* vkImg = VK_GetImage(texture);
        if (!vkImg || vkImg->descriptorSet == VK_NULL_HANDLE) {
            continue;  // Skip stage without valid descriptor set to avoid using stale descriptor set
        }

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               g_vkPipelines.pipelineLayout, 2, 1,
                               &vkImg->descriptorSet, 0, nullptr);

        // Draw
        VK_DrawIndexed(input->numIndexes, 0, 0);
    }
}

//----------------------------------------------------------------------------
// Draw stage vertex lit texture
//----------------------------------------------------------------------------
void VK_DrawStageVertexLitTexture(const shaderCommands_t* input) {
    // Same as generic for now
    VK_DrawStageGeneric(input);
}

//----------------------------------------------------------------------------
// Draw stage lightmapped multitexture
//----------------------------------------------------------------------------
void VK_DrawStageLightmappedMultitexture(const shaderCommands_t* input) {
    // Same as generic for now
    VK_DrawStageGeneric(input);
}

//----------------------------------------------------------------------------
// Begin tessellate
//----------------------------------------------------------------------------
void VK_BeginTessellate(const shaderCommands_t* input) {
    // No-op - tessellation handled in draw functions
}

//----------------------------------------------------------------------------
// End tessellate
//----------------------------------------------------------------------------
void VK_EndTessellate(const shaderCommands_t* input) {
    // No-op - tessellation handled in draw functions
}

//----------------------------------------------------------------------------
// Debug: Draw axis
//----------------------------------------------------------------------------
void VK_DebugDrawAxis(void) {
    // TODO: Implement debug axis rendering
}

//----------------------------------------------------------------------------
// Debug: Draw normals
//----------------------------------------------------------------------------
void VK_DebugDrawNormals(const shaderCommands_t* input) {
    // TODO: Implement debug normal rendering
}

//----------------------------------------------------------------------------
// Debug: Draw triangles
//----------------------------------------------------------------------------
void VK_DebugDrawTris(const shaderCommands_t* input) {
    // TODO: Implement debug wireframe rendering
}

//----------------------------------------------------------------------------
// Debug: Set overdraw measure enabled
//----------------------------------------------------------------------------
void VK_DebugSetOverdrawMeasureEnabled(qboolean enabled) {
    // TODO: Implement overdraw measurement
}

//----------------------------------------------------------------------------
// Debug: Set texture mode
//----------------------------------------------------------------------------
void VK_DebugSetTextureMode(const char* mode) {
    // TODO: Implement texture debug modes
}

//----------------------------------------------------------------------------
// Debug: Draw polygon
//----------------------------------------------------------------------------
void VK_DebugDrawPolygon(int color, int numPoints, const float* points) {
    // TODO: Implement debug polygon rendering
}
