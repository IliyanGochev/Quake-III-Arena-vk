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
    g_vkRunState.viewVSDirty = qtrue;
    g_vkRunState.viewPSDirty = qtrue;

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
    if (g_vkDraw.inRenderPass) {
        VK_EndFrame();
    }
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
            ri.Printf(PRINT_WARNING, "VK_DrawImage: WARNING - viewport not set!\n");
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
    float positions[4][2];
    float texCoordsData[4][2];

    // Bottom-left
    positions[0][0] = coords[0];
    positions[0][1] = coords[3];
    texCoordsData[0][0] = texcoords[0];
    texCoordsData[0][1] = texcoords[3];

    // Bottom-right
    positions[1][0] = coords[2];
    positions[1][1] = coords[3];
    texCoordsData[1][0] = texcoords[2];
    texCoordsData[1][1] = texcoords[3];

    // Top-right
    positions[2][0] = coords[2];
    positions[2][1] = coords[1];
    texCoordsData[2][0] = texcoords[2];
    texCoordsData[2][1] = texcoords[1];

    // Top-left
    positions[3][0] = coords[0];
    positions[3][1] = coords[1];
    texCoordsData[3][0] = texcoords[0];
    texCoordsData[3][1] = texcoords[1];

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
        ri.Printf(PRINT_WARNING, "WARNING: Texture descriptor set is NULL!\n");
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
    // OpenGL NDC: Z_ndc = m[10]*z + m[14] (after division by W)
    // We want: Z_vulkan = (Z_opengl + 1) / 2 = 0.5 * Z_opengl + 0.5
    // Therefore:
    //   m_vk[10] = 0.5 * m_gl[10]
    //   m_vk[14] = 0.5 * m_gl[14] + 0.5
    g_vkRunState.projectionMatrix[10] = projMatrix[10] * 0.5f;
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

    VkViewport viewport = {};
    viewport.x = (float)left;
    // For negative height (Y-flip), viewport.y must be the BOTTOM of the region
    // This makes NDC y=-1 map to (top) and y=+1 map to (top + height)
    viewport.y = (float)(top + height);
    viewport.width = (float)width;
    viewport.height = -(float)height;  // Negative for Y-flip
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Update scissor to match
    VkRect2D scissor = {};
    scissor.offset = {left, top};
    scissor.extent = {(uint32_t)width, (uint32_t)height};

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
    // Store state
    g_vkRunState.stateMask = stateMask;

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
}

//----------------------------------------------------------------------------
// Reset state for 3D rendering
//----------------------------------------------------------------------------
void VK_ResetState3D(void) {
    g_vkRunState.stateMask = GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE;
    g_vkRunState.cullMode = CT_FRONT_SIDED;
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
    // TODO: Implement skybox rendering
    ri.Printf(PRINT_DEVELOPER, "VK_DrawSkyBox called\n");
}

//----------------------------------------------------------------------------
// Draw beam
//----------------------------------------------------------------------------
void VK_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs) {
    // TODO: Implement beam rendering (lightning, etc.)
    ri.Printf(PRINT_DEVELOPER, "VK_DrawBeam not yet implemented\n");
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
        key.depthFlags = pStage->stateBits & (GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE | GLS_DEPTHFUNC_EQUAL);
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
        if (texture) {
            vkImage_t* vkImg = VK_GetImage(texture);
            if (vkImg && vkImg->descriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       g_vkPipelines.pipelineLayout, 2, 1,
                                       &vkImg->descriptorSet, 0, nullptr);
            }
        }

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
