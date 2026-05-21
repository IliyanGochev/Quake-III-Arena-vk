#include "vk_common.h"
#include "vk_driver.h"
#include "vk_state.h"
#include "vk_drawdata.h"

//----------------------------------------------------------------------------
// VKDrv_Clear -- issues clear commands
//----------------------------------------------------------------------------

void VKDrv_Clear( unsigned long bits, const float* clearCol, unsigned long stencil, float depth )
{
    // Acquire swapchain image at start of frame (once per frame)
    VKDRV_AcquireNextImage();

    // Begin primary command buffer for this frame
    VKDRV_BeginFrame();

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    // Render pass has 2 attachments: color (index 0) + depth/stencil (index 1)
    // Always provide clear values for both to avoid out-of-bounds reads
    VkClearValue clearValues[2] = {};

    clearValues[0].color.float32[0] = clearCol ? clearCol[0] : 0;
    clearValues[0].color.float32[1] = clearCol ? clearCol[1] : 0;
    clearValues[0].color.float32[2] = clearCol ? clearCol[2] : 0;
    clearValues[0].color.float32[3] = clearCol ? clearCol[3] : 0;

    clearValues[1].depthStencil.depth = depth;
    clearValues[1].depthStencil.stencil = (uint32_t)stencil;

    // Begin render pass with clear
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = g_vkRenderPass;
    beginInfo.framebuffer = g_vkFramebuffers[g_vkCurrentImageIndex];
    beginInfo.renderArea.offset = { g_vkRunState.viewportLeft, g_vkRunState.viewportTop };
    beginInfo.renderArea.extent = { (uint32_t)g_vkRunState.viewportWidth, (uint32_t)g_vkRunState.viewportHeight };
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass( cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
    g_vkRenderPassActive = qtrue;

    // Debug label for render phase (issue #6)
    VKDRV_BeginDebugLabel( cmd, "RenderFrame" );

    // Set dynamic state after render pass begins
    VkViewport viewport = {};
    viewport.x = (float)g_vkRunState.viewportLeft;
    viewport.y = (float)(g_vkRunState.viewportTop + g_vkRunState.viewportHeight);
    viewport.width = (float)g_vkRunState.viewportWidth;
    viewport.height = -(float)g_vkRunState.viewportHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport( cmd, 0, 1, &viewport );

    VkRect2D scissor = {};
    scissor.offset = { g_vkRunState.viewportLeft, g_vkRunState.viewportTop };
    scissor.extent = { (uint32_t)g_vkRunState.viewportWidth, (uint32_t)g_vkRunState.viewportHeight };
    vkCmdSetScissor( cmd, 0, 1, &scissor );

    // Set depth bounds (full range by default)
    vkCmdSetDepthBounds( cmd, 0.0f, 1.0f );
}

//----------------------------------------------------------------------------
// VKDrv_SetProjection / SetModelView
//----------------------------------------------------------------------------

void VKDrv_SetProjection( const float* projMatrix )
{
    if ( projMatrix )
    {
        Com_Memcpy( g_vkRunState.vsConstants.projectionMatrix, projMatrix, sizeof(float) * 16 );
    }
    g_vkRunState.vsDirtyConstants = qtrue;
}

void VKDrv_GetProjection( float* projMatrix )
{
    if ( projMatrix )
    {
        Com_Memcpy( projMatrix, g_vkRunState.vsConstants.projectionMatrix, sizeof(float) * 16 );
    }
}

void VKDrv_SetModelView( const float* modelViewMatrix )
{
    if ( modelViewMatrix )
    {
        Com_Memcpy( g_vkRunState.vsConstants.modelViewMatrix, modelViewMatrix, sizeof(float) * 16 );
    }
    g_vkRunState.vsDirtyConstants = qtrue;
}

void VKDrv_GetModelView( float* modelViewMatrix )
{
    if ( modelViewMatrix )
    {
        Com_Memcpy( modelViewMatrix, g_vkRunState.vsConstants.modelViewMatrix, sizeof(float) * 16 );
    }
}

//----------------------------------------------------------------------------
// VKDrv_SetViewport
//----------------------------------------------------------------------------

void VKDrv_SetViewport( int left, int top, int width, int height )
{
    g_vkRunState.viewportLeft = left;
    g_vkRunState.viewportTop = top;
    g_vkRunState.viewportWidth = width;
    g_vkRunState.viewportHeight = height;

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    // Y-inversion via negative viewport height (VK_KHR_maintenance1)
    VkViewport viewport = {};
    viewport.x = (float)left;
    viewport.y = (float)(top + height);
    viewport.width = (float)width;
    viewport.height = -(float)height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { left, top };
    scissor.extent = { (uint32_t)width, (uint32_t)height };

    vkCmdSetViewport( cmd, 0, 1, &viewport );
    vkCmdSetScissor( cmd, 0, 1, &scissor );
}

//----------------------------------------------------------------------------
// VKDrv_SetDepthRange
//----------------------------------------------------------------------------

void VKDrv_SetDepthRange( float minRange, float maxRange )
{
    g_vkRunState.vsConstants.depthRange[0] = minRange;
    g_vkRunState.vsConstants.depthRange[1] = maxRange;
    g_vkRunState.vsDirtyConstants = qtrue;

    // vkCmdSetDepthRange was removed in Vulkan 1.1+; depth range is now
    // baked into the pipeline. The values above are used by the vertex shader.
}

//----------------------------------------------------------------------------
// VKDrv_SetPortalRendering
//----------------------------------------------------------------------------

void VKDrv_SetPortalRendering( qboolean enabled, const float* flipMatrix, const float* plane )
{
    (void)plane;

    // Portal rendering: apply flip matrix to create mirrored view
    // Full portal rendering requires an off-screen framebuffer with its own
    // depth/stencil attachment, which would need framebuffer recreation.
    // For now, apply the flipped projection matrix for the mirrored view.
    if ( enabled && flipMatrix )
    {
        Com_Memcpy( g_vkRunState.vsConstants.projectionMatrix, flipMatrix, sizeof(float) * 16 );
        g_vkRunState.vsDirtyConstants = qtrue;
    }
    else if ( !enabled )
    {
        // When portal rendering is disabled, the engine will restore the
        // original projection matrix via the normal SetProjection call.
    }
}

//----------------------------------------------------------------------------
// VKDrv_SetDrawBuffer
//----------------------------------------------------------------------------

void VKDrv_SetDrawBuffer( int buffer )
{
    // In Vulkan, render targets are set via the render pass
    // This is a no-op for single-backbuffer rendering
}

//----------------------------------------------------------------------------
// VKDrv_Flush -- ensures all commands are submitted
//----------------------------------------------------------------------------

void VKDrv_Flush( void )
{
    // Wait for the current frame's GPU work to complete via its in-flight fence.
    // This avoids a full-device stall (vkDeviceWaitIdle) while still ensuring
    // all recorded commands are finished before the caller proceeds.
    vkWaitForFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame], VK_TRUE, UINT64_MAX );
}

//----------------------------------------------------------------------------
// VKDrv_EndFrame -- submits and presents the swapchain image
//----------------------------------------------------------------------------

void VKDrv_EndFrame( void )
{
    VKDRV_SubmitAndPresent();
}

//----------------------------------------------------------------------------
// VKDrv_MakeCurrent
//----------------------------------------------------------------------------

void VKDrv_MakeCurrent( qboolean current )
{
    // Not applicable to Vulkan's command buffer model
}

//----------------------------------------------------------------------------
// VKDrv_ReadPixels
//----------------------------------------------------------------------------

void VKDrv_ReadPixels( int x, int y, int width, int height, imageFormat_t requestedFmt, void* dest )
{
    // Read from current framebuffer
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    // Transition swapchain image to GENERAL layout (allowed for swapchain images
    // and supports transfer reads, unlike TRANSFER_SRC_OPTIMAL).
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = g_vkSwapchainImages[g_vkCurrentImageIndex];
    barrier.subresourceRange = subresourceRange;

    VkCommandBuffer cmd = VKDRV_BeginCommandBuffer();
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    // Create staging buffer
    VkDeviceSize bufferSize = width * height * 4;
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufInfo, &allocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr ) );

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = { (int32_t)x, (int32_t)y, 0 };
    copyRegion.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyImageToBuffer( cmd, g_vkSwapchainImages[g_vkCurrentImageIndex],
                             VK_IMAGE_LAYOUT_GENERAL,
                             stagingBuffer, 1, &copyRegion );

    // Transition back to PRESENT_SRC_KHR
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    VKDRV_EndCommandBuffer( cmd );
    VKDRV_SubmitCommandBuffer( cmd, qtrue, qtrue ); // wait for transfer before reading

    // Copy from mapped memory with BGR→RGB conversion if needed.
    // Swapchain is B8G8R8A8 but engine expects RGBA ordering.
    // Vulkan automatically converts sRGB→linear on image reads.
    void* mapped;
    vmaMapMemory( g_vkAllocator, stagingAllocation, &mapped );

    if ( g_vkSwapchainFormat == VK_FORMAT_B8G8R8A8_SRGB ||
         g_vkSwapchainFormat == VK_FORMAT_B8G8R8A8_UNORM )
    {
        // Swap R and B channels for B8G8R8A8 → R8G8B8A8
        const uint32_t* src = (const uint32_t*)mapped;
        uint32_t* dst = (uint32_t*)dest;
        for ( int p = 0; p < width * height; p++ )
        {
            uint32_t px = src[p];
            // Swap bytes 0 and 2 (B↔R on little-endian)
            dst[p] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
        }
    }
    else
    {
        memcpy( dest, mapped, (size_t)bufferSize );
    }
    vmaUnmapMemory( g_vkAllocator, stagingAllocation );

    vmaDestroyBuffer( g_vkAllocator, stagingBuffer, stagingAllocation );
}

//----------------------------------------------------------------------------
// VKDrv_ReadDepth
//----------------------------------------------------------------------------

void VKDrv_ReadDepth( int x, int y, int width, int height, float* dest )
{
    // Simplified: read from depth image
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    VkDeviceSize bufferSize = width * height * sizeof(float);
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufInfo, &allocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr ) );

    VkCommandBuffer cmd = VKDRV_BeginCommandBuffer();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = g_vkDepthImage;
    barrier.subresourceRange = subresourceRange;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = { (int32_t)x, (int32_t)y, 0 };
    copyRegion.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyImageToBuffer( cmd, g_vkDepthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                             stagingBuffer, 1, &copyRegion );

    VKDRV_EndCommandBuffer( cmd );
    VKDRV_SubmitCommandBuffer( cmd, qtrue, qtrue ); // wait for transfer before reading

    void* mapped;
    vmaMapMemory( g_vkAllocator, stagingAllocation, &mapped );
    memcpy( dest, mapped, (size_t)bufferSize );
    vmaUnmapMemory( g_vkAllocator, stagingAllocation );

    vmaDestroyBuffer( g_vkAllocator, stagingBuffer, stagingAllocation );
}

//----------------------------------------------------------------------------
// VKDrv_ReadStencil
//----------------------------------------------------------------------------

void VKDrv_ReadStencil( int x, int y, int width, int height, byte* dest )
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    VkDeviceSize bufferSize = width * height;
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufInfo, &allocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr ) );

    VkCommandBuffer cmd = VKDRV_BeginCommandBuffer();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = g_vkDepthImage;
    barrier.subresourceRange = subresourceRange;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = { (int32_t)x, (int32_t)y, 0 };
    copyRegion.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyImageToBuffer( cmd, g_vkDepthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                             stagingBuffer, 1, &copyRegion );

    VKDRV_EndCommandBuffer( cmd );
    VKDRV_SubmitCommandBuffer( cmd, qtrue, qtrue ); // wait for transfer before reading

    void* mapped;
    vmaMapMemory( g_vkAllocator, stagingAllocation, &mapped );
    memcpy( dest, mapped, (size_t)bufferSize );
    vmaUnmapMemory( g_vkAllocator, stagingAllocation );

    vmaDestroyBuffer( g_vkAllocator, stagingBuffer, stagingAllocation );
}

//----------------------------------------------------------------------------
// VKDrv_SetGamma
//----------------------------------------------------------------------------

// Gamma LUT stored here for texture upload (Vulkan has no hardware gamma)
// Defined in vk_common.h as extern; actual storage is here
unsigned char g_vkGammaTable[256];

void VKDrv_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
    // Per spec §11.6, replicate the OpenGL driver's table-based gamma approach.
    // The engine applies s_intensitytable (overbright) during R_LoadImage,
    // then passes s_gammatable via GFX_SetGamma. We store the gamma table
    // and apply it during texture upload since Vulkan has no hardware gamma.
    if ( red )
        Com_Memcpy( g_vkGammaTable, red, sizeof( g_vkGammaTable ) );
    else
    {
        // Identity fallback
        for ( int i = 0; i < 256; i++ )
            g_vkGammaTable[i] = (unsigned char)i;
    }
}

//----------------------------------------------------------------------------
// VKDrv_GetFrameImageMemoryUsage
//----------------------------------------------------------------------------

int VKDrv_GetFrameImageMemoryUsage( void )
{
    // Return approximate memory usage tracked by VMA
    VmaTotalStatistics stats;
    vmaCalculateStatistics( g_vkAllocator, &stats );
    return (int)(stats.total.statistics.blockBytes / (1024 * 1024)); // MB
}

//----------------------------------------------------------------------------
// VKDrv_GfxInfo -- prints graphics info
//----------------------------------------------------------------------------

void VKDrv_GfxInfo( void )
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties( g_vkPhysicalDevice, &props );

    ri.Printf( PRINT_DEVELOPER, "--- Vulkan Hardware Info ---\n" );
    ri.Printf( PRINT_DEVELOPER, "Driver: %s\n", props.deviceName );

    const char* typeStr = "Unknown";
    switch ( props.deviceType )
    {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeStr = "Integrated GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:  typeStr = "Discrete GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:   typeStr = "Virtual GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:           typeStr = "CPU"; break;
    }
    ri.Printf( PRINT_DEVELOPER, "Type: %s\n", typeStr );
    ri.Printf( PRINT_DEVELOPER, "API Version: %d.%d.%d\n",
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion),
               VK_VERSION_PATCH(props.apiVersion) );
}

//----------------------------------------------------------------------------
// VKDrv_ShadowSilhouette / ShadowFinish
//----------------------------------------------------------------------------

// Shadow volume silhouette edges (extruded to depth for stencil shadows)
static float* g_vkShadowEdges = nullptr;
static int    g_vkShadowEdgeCount = 0;

void VKDrv_ShadowSilhouette( const float* edges, int edgeCount )
{
    // Store silhouette edges for stencil shadow volume rendering
    if ( g_vkShadowEdges )
    {
        ri.Free( g_vkShadowEdges );
        g_vkShadowEdges = nullptr;
    }

    if ( !edges || edgeCount <= 0 )
        return;

    // Each edge: 6 floats (v0[x,y,z], v1[x,y,z])
    g_vkShadowEdges = (float*)ri.Malloc( sizeof(float) * edgeCount * 6 );
    Com_Memcpy( g_vkShadowEdges, edges, sizeof(float) * edgeCount * 6 );
    g_vkShadowEdgeCount = edgeCount;
}

void VKDrv_ShadowFinish( void )
{
    if ( !g_vkShadowEdges || g_vkShadowEdgeCount == 0 )
    {
        return;
    }

    // Render stencil shadow volumes from stored silhouette edges.
    // Each edge forms a quad extruded along the light direction.
    // Uses the dedicated shadow pipeline with proper stencil state.
    int vertCount = g_vkShadowEdgeCount * 4;
    float* shadowVerts = (float*)ri.Malloc( vertCount * sizeof(vec4_t) );
    if ( !shadowVerts )
    {
        ri.Printf( PRINT_WARNING, "Vulkan: Out of memory in VKDrv_ShadowFinish\n" );
        ri.Free( g_vkShadowEdges );
        g_vkShadowEdges = nullptr;
        g_vkShadowEdgeCount = 0;
        return;
    }

    // Extrude far enough to cover the view frustum.
    // Vertices are in view space; extrude along view direction (+Z = away).
    float extrude = 2000.0f;
    int vi = 0;

    for ( int i = 0; i < g_vkShadowEdgeCount * 6; i += 6 )
    {
        float v0x = g_vkShadowEdges[i + 0];
        float v0y = g_vkShadowEdges[i + 1];
        float v0z = g_vkShadowEdges[i + 2];
        float v1x = g_vkShadowEdges[i + 3];
        float v1y = g_vkShadowEdges[i + 4];
        float v1z = g_vkShadowEdges[i + 5];

        // Near plane edge vertices
        shadowVerts[vi * 4] = v0x; shadowVerts[vi * 4 + 1] = v0y;
        shadowVerts[vi * 4 + 2] = v0z; shadowVerts[vi * 4 + 3] = 1; vi++;
        shadowVerts[vi * 4] = v1x; shadowVerts[vi * 4 + 1] = v1y;
        shadowVerts[vi * 4 + 2] = v1z; shadowVerts[vi * 4 + 3] = 1; vi++;
        // Far plane edge vertices (extruded along view direction)
        shadowVerts[vi * 4] = v0x; shadowVerts[vi * 4 + 1] = v0y;
        shadowVerts[vi * 4 + 2] = v0z + extrude; shadowVerts[vi * 4 + 3] = 1; vi++;
        shadowVerts[vi * 4] = v1x; shadowVerts[vi * 4 + 1] = v1y;
        shadowVerts[vi * 4 + 2] = v1z + extrude; shadowVerts[vi * 4 + 3] = 1; vi++;
    }

    // Upload shadow vertices to tessellation buffer
    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, shadowVerts, vertCount * sizeof(vec4_t) );
    ri.Free( shadowVerts );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    // Set stencil reference for shadow volume rendering
    vkCmdSetStencilReference( cmd, VK_STENCIL_FRONT_AND_BACK, 1 );

    // Draw shadow volume quads using the dedicated shadow pipeline
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkDrawState.quadRenderData.shadowPipeline );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDraw( cmd, vertCount, 1, 0, 0 );

    // Reset stencil reference after shadow rendering
    vkCmdSetStencilReference( cmd, VK_STENCIL_FRONT_AND_BACK, 0 );

    ri.Free( g_vkShadowEdges );
    g_vkShadowEdges = nullptr;
    g_vkShadowEdgeCount = 0;
}
