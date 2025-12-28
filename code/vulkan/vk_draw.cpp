// Vulkan rendering backend - draw commands implementation

#include "vk_draw.h"
#include "vk_state.h"
#include "vk_image.h"
#include "vk_buffers.h"
#include "vk_shaders.h"
#include <string.h>
#include <math.h>

//=============================================================================
// Vertex formats
//=============================================================================

typedef struct vkGenericVertex_s {
    float position[4];
    float texCoord0[2];
    float texCoord1[2];
    byte color[4];
} vkGenericVertex_t;

typedef struct vk2DVertex_s {
    float position[2];
    float texCoord[2];
    byte color[4];
} vk2DVertex_t;

// Skybox vertex: position[3] + texcoord[2] = 20 bytes (matching D3D11)
typedef struct vkSkyboxVertex_s {
    float position[3];
    float texCoord[2];
} vkSkyboxVertex_t;

//=============================================================================
// Skybox static data (matching D3D11 skybox geometry)
//=============================================================================

static const float s_skyboxVertexData[] = {
    // Right (side 0)
     1, -1, -1, 1, 1,
     1, -1,  1, 1, 0,
     1,  1,  1, 0, 0,
     1, -1, -1, 1, 1,
     1,  1,  1, 0, 0,
     1,  1, -1, 0, 1,
    // Left (side 1)
    -1, -1,  1, 0, 0,
    -1, -1, -1, 0, 1,
    -1,  1, -1, 1, 1,
    -1, -1,  1, 0, 0,
    -1,  1, -1, 1, 1,
    -1,  1,  1, 1, 0,
    // Back (side 2)
    -1,  1,  1, 0, 0,
    -1,  1, -1, 0, 1,
     1,  1, -1, 1, 1,
    -1,  1,  1, 0, 0,
     1,  1, -1, 1, 1,
     1,  1,  1, 1, 0,
    // Front (side 3)
     1, -1,  1, 0, 0,
     1, -1, -1, 0, 1,
    -1, -1, -1, 1, 1,
     1, -1,  1, 0, 0,
    -1, -1, -1, 1, 1,
    -1, -1,  1, 1, 0,
    // Up (side 4)
     1, -1,  1, 1, 1,
    -1, -1,  1, 1, 0,
    -1,  1,  1, 0, 0,
     1, -1,  1, 1, 1,
    -1,  1,  1, 0, 0,
     1,  1,  1, 0, 1,
    // Down (side 5)
    -1, -1, -1, 1, 0,
     1, -1, -1, 1, 1,
     1,  1, -1, 0, 1,
    -1, -1, -1, 1, 0,
     1,  1, -1, 0, 1,
    -1,  1, -1, 0, 0,
};

static VkBuffer s_skyboxVertexBuffer = VK_NULL_HANDLE;
static VmaAllocation s_skyboxVertexAllocation = VK_NULL_HANDLE;

//=============================================================================
// Initialization
//=============================================================================

static void CreateSkyboxBuffer(void)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(s_skyboxVertexData);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Use CPU-accessible memory for simplicity (small static buffer)
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo;
    VkResult result = vmaCreateBuffer(g_vmaAllocator, &bufferInfo, &allocInfo,
        &s_skyboxVertexBuffer, &s_skyboxVertexAllocation, &allocationInfo);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: Failed to create skybox vertex buffer: %s\n", Vk_ResultString(result));
        return;
    }

    // Copy data directly (memory is mapped)
    memcpy(allocationInfo.pMappedData, s_skyboxVertexData, sizeof(s_skyboxVertexData));
    Com_Printf("Created skybox vertex buffer (%d bytes)\n", (int)sizeof(s_skyboxVertexData));
}

static void DestroySkyboxBuffer(void)
{
    if (s_skyboxVertexBuffer) {
        vmaDestroyBuffer(g_vmaAllocator, s_skyboxVertexBuffer, s_skyboxVertexAllocation);
        s_skyboxVertexBuffer = VK_NULL_HANDLE;
        s_skyboxVertexAllocation = VK_NULL_HANDLE;
    }
}

void VkDraw_Init(void)
{
    CreateSkyboxBuffer();
}

void VkDraw_Shutdown(void)
{
    DestroySkyboxBuffer();
}

//=============================================================================
// Frame management
//=============================================================================

qboolean VkDraw_BeginFrame(void)
{
    if (vk.inFrame) {
        return qtrue;
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];

    // Wait for this frame's fence
    qvkWaitForFences(vk.device, 1, &frame->inFlightFence, VK_TRUE, UINT64_MAX);
    qvkResetFences(vk.device, 1, &frame->inFlightFence);

    // Reset the dynamic descriptor pool for this frame (safe now that fence is signaled)
    if (frame->dynamicDescriptorPool) {
        qvkResetDescriptorPool(vk.device, frame->dynamicDescriptorPool, 0);
    }

    // Acquire next swapchain image
    VkResult result = qvkAcquireNextImageKHR(vk.device, vk.swapchain.handle, UINT64_MAX,
        frame->imageAvailableSemaphore, VK_NULL_HANDLE, &vk.currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        Vk_RecreateSwapchain();
        return qfalse;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        Com_Printf("ERROR: vkAcquireNextImageKHR failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    // Reset and begin command buffer
    qvkResetCommandBuffer(frame->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = qvkBeginCommandBuffer(frame->commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkBeginCommandBuffer failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    frame->commandBufferRecording = qtrue;
    vk.inFrame = qtrue;

    return qtrue;
}

static void BeginRenderPass(const float* clearColor, float clearDepth)
{
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];

    // Don't begin render pass if already in one
    if (frame->inRenderPass) {
        return;
    }

    VkClearValue clearValues[2];
    if (clearColor) {
        clearValues[0].color.float32[0] = clearColor[0];
        clearValues[0].color.float32[1] = clearColor[1];
        clearValues[0].color.float32[2] = clearColor[2];
        clearValues[0].color.float32[3] = clearColor[3];
    } else {
        // Default to black if no clear color specified
        clearValues[0].color.float32[0] = 0.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 0.0f;
        clearValues[0].color.float32[3] = 1.0f;
    }
    clearValues[1].depthStencil.depth = clearDepth;
    clearValues[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vk.renderPass;
    renderPassInfo.framebuffer = vk.framebuffers[vk.currentImageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = vk.swapchain.extent;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    qvkCmdBeginRenderPass(frame->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    frame->inRenderPass = qtrue;

    // Set default viewport and scissor (standard positive height)
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk.swapchain.extent.width;
    viewport.height = (float)vk.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    qvkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = vk.swapchain.extent;
    qvkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);
}

//=============================================================================
// Clear
//=============================================================================

void VkDraw_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth)
{
    // Ensure we're in a frame
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];

    // If not in render pass, start one with the clear values
    if (!frame->inRenderPass) {
        BeginRenderPass(clearCol, depth);
        return;
    }

    // Already in render pass - use vkCmdClearAttachments for mid-pass clear
    VkClearAttachment clearAttachments[2];
    uint32_t attachmentCount = 0;

    // Clear color if requested
    if ((bits & CLEAR_COLOR) && clearCol) {
        clearAttachments[attachmentCount].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachments[attachmentCount].colorAttachment = 0;
        clearAttachments[attachmentCount].clearValue.color.float32[0] = clearCol[0];
        clearAttachments[attachmentCount].clearValue.color.float32[1] = clearCol[1];
        clearAttachments[attachmentCount].clearValue.color.float32[2] = clearCol[2];
        clearAttachments[attachmentCount].clearValue.color.float32[3] = clearCol[3];
        attachmentCount++;
    }

    // Clear depth if requested
    if (bits & CLEAR_DEPTH) {
        clearAttachments[attachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clearAttachments[attachmentCount].colorAttachment = 0;
        clearAttachments[attachmentCount].clearValue.depthStencil.depth = depth;
        clearAttachments[attachmentCount].clearValue.depthStencil.stencil = (uint32_t)stencil;
        attachmentCount++;
    }

    if (attachmentCount > 0) {
        VkClearRect clearRect = {};
        clearRect.rect.offset = { 0, 0 };
        clearRect.rect.extent = vk.swapchain.extent;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;

        qvkCmdClearAttachments(frame->commandBuffer, attachmentCount, clearAttachments, 1, &clearRect);
    }
}

//=============================================================================
// 2D Drawing
//=============================================================================

void VkDraw_Image(const image_t* image, const float* coords, const float* texcoords, const float* color)
{
    if (!image) {
        return;
    }

    // Ensure we're in a frame and render pass
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];
    if (!frame->inRenderPass) {
        BeginRenderPass(NULL, 1.0f);
    }

    // Get image descriptor set
    VkDescriptorSet texSet = VkImage_GetDescriptorSet(image);
    if (!texSet) {
        return;
    }

    // coords[4] = {x1, y1, x2, y2} - screen coordinates (min/max corners)
    // texcoords[4] = {s1, t1, s2, t2} - texture coordinates (min/max)
    // Convert screen coords to NDC (-1 to 1)
    float x1 = coords[0];
    float y1 = coords[1];
    float x2 = coords[2];
    float y2 = coords[3];
    float s1 = texcoords[0];
    float t1 = texcoords[1];
    float s2 = texcoords[2];
    float t2 = texcoords[3];

    // Convert from screen space (0 to width/height) to NDC (-1 to 1)
    // NDC x = (screen_x / width) * 2 - 1
    // NDC y = (screen_y / height) * 2 - 1 (Vulkan Y is top-down like screen coords)
    float width = (float)vk.swapchain.extent.width;
    float height = (float)vk.swapchain.extent.height;

    float ndcX1 = (x1 / width) * 2.0f - 1.0f;
    float ndcY1 = (y1 / height) * 2.0f - 1.0f;
    float ndcX2 = (x2 / width) * 2.0f - 1.0f;
    float ndcY2 = (y2 / height) * 2.0f - 1.0f;

    // Allocate vertex data
    vk2DVertex_t vertices[4];

    // Quad vertices (matching OpenGL's GL_QUADS winding order):
    // 0--1
    // |  |
    // 3--2
    // Vertex 0: top-left
    vertices[0].position[0] = ndcX1;
    vertices[0].position[1] = ndcY1;
    vertices[0].texCoord[0] = s1;
    vertices[0].texCoord[1] = t1;

    // Vertex 1: top-right
    vertices[1].position[0] = ndcX2;
    vertices[1].position[1] = ndcY1;
    vertices[1].texCoord[0] = s2;
    vertices[1].texCoord[1] = t1;

    // Vertex 2: bottom-right
    vertices[2].position[0] = ndcX2;
    vertices[2].position[1] = ndcY2;
    vertices[2].texCoord[0] = s2;
    vertices[2].texCoord[1] = t2;

    // Vertex 3: bottom-left
    vertices[3].position[0] = ndcX1;
    vertices[3].position[1] = ndcY2;
    vertices[3].texCoord[0] = s1;
    vertices[3].texCoord[1] = t2;

    // Set vertex colors
    byte r = color ? (byte)(color[0] * 255) : 255;
    byte g = color ? (byte)(color[1] * 255) : 255;
    byte b = color ? (byte)(color[2] * 255) : 255;
    byte a = 255;
    for (int i = 0; i < 4; i++) {
        vertices[i].color[0] = r;
        vertices[i].color[1] = g;
        vertices[i].color[2] = b;
        vertices[i].color[3] = a;
    }

    // Allocate and upload
    vkBufferAlloc_t vertAlloc = VkBuffers_AllocVertex(sizeof(vertices));
    if (!vertAlloc.data) {
        return;
    }
    memcpy(vertAlloc.data, vertices, sizeof(vertices));

    // Index buffer for two triangles
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    vkBufferAlloc_t idxAlloc = VkBuffers_AllocIndex(sizeof(indices));
    if (!idxAlloc.data) {
        return;
    }
    memcpy(idxAlloc.data, indices, sizeof(indices));

    // Get 2D pipeline (uses different vertex format and shaders)
    VkPipeline pipeline = VkState_Get2DPipeline();

    if (pipeline) {
        // Allocate dynamic uniforms from ring buffer
        uint32_t vsOffset, psOffset;
        if (!VkState_AllocDynamicUniforms(&vsOffset, &psOffset)) {
            return;  // Failed to allocate uniform buffer space
        }

        // IMPORTANT: Reset viewport to positive height for 2D rendering
        // 3D rendering uses negative height viewport for Y-flip, but 2D uses NDC directly
        VkViewport viewport2D = {};
        viewport2D.x = 0.0f;
        viewport2D.y = 0.0f;
        viewport2D.width = (float)vk.swapchain.extent.width;
        viewport2D.height = (float)vk.swapchain.extent.height;
        viewport2D.minDepth = 0.0f;
        viewport2D.maxDepth = 1.0f;
        qvkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport2D);

        qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind vertex/index buffers
        VkDeviceSize offsets[] = { vertAlloc.offset };
        qvkCmdBindVertexBuffers(frame->commandBuffer, 0, 1, &vertAlloc.buffer, offsets);
        qvkCmdBindIndexBuffer(frame->commandBuffer, idxAlloc.buffer, idxAlloc.offset, VK_INDEX_TYPE_UINT16);

        // Bind texture descriptor set with dynamic UBO offsets
        uint32_t dynamicOffsets[2] = { vsOffset, psOffset };
        qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            VkState_GetPipelineLayout(), 0, 1, &texSet, 2, dynamicOffsets);

        // Draw
        qvkCmdDrawIndexed(frame->commandBuffer, 6, 1, 0, 0, 0);
    }
}

//=============================================================================
// Stage iteration (main 3D rendering)
//=============================================================================

void VkDraw_StageGeneric(const shaderCommands_t* input)
{
    if (!input || input->numVertexes == 0 || input->numIndexes == 0) {
        return;
    }

    // Skip drawing if projection matrix hasn't been set yet (all zeros = invalid)
    // This prevents rendering with garbage transforms at frame start
    if (g_vkViewState.projectionMatrix[0] == 0.0f &&
        g_vkViewState.projectionMatrix[5] == 0.0f &&
        g_vkViewState.projectionMatrix[10] == 0.0f) {
        return;
    }

    // Ensure we're in a frame and render pass
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];
    if (!frame->inRenderPass) {
        BeginRenderPass(NULL, 1.0f);
    }

    // Allocate and upload index data (shared across all stages)
    size_t idxSize = input->numIndexes * sizeof(glIndex_t);
    vkBufferAlloc_t idxAlloc = VkBuffers_AllocIndex(idxSize);
    if (!idxAlloc.data) {
        return;
    }
    memcpy(idxAlloc.data, input->indexes, idxSize);

    // Set viewport based on current state (fallback to swapchain size if not set)
    int vpWidth = g_vkPipelineState.viewportWidth;
    int vpHeight = g_vkPipelineState.viewportHeight;
    if (vpWidth <= 0 || vpHeight <= 0) {
        vpWidth = vk.swapchain.extent.width;
        vpHeight = vk.swapchain.extent.height;
    }

    VkViewport viewport = {};
    viewport.x = (float)g_vkPipelineState.viewportX;
    // Convert OpenGL viewport Y (bottom-up origin) to Vulkan (top-down origin)
    viewport.y = (float)(vk.swapchain.extent.height - g_vkPipelineState.viewportY);
    viewport.width = (float)vpWidth;
    viewport.height = -(float)vpHeight;
    // Depth range is applied in shader via uniform - viewport should use [0,1]
    // to avoid double-applying the depth range
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    qvkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

    // Use full-screen scissor (D3D11 disables scissor for viewport changes)
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = vk.swapchain.extent.width;
    scissor.extent.height = vk.swapchain.extent.height;
    qvkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

    // Iterate through shader stages using xstages (the active stage list)
    for (int stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        shaderStage_t* pStage = input->xstages[stage];
        if (!pStage) {
            break;
        }

        // Set state bits for this stage (like D3D11 does)
        // This may update alpha test uniforms
        VkState_SetState(pStage->stateBits);

        // Allocate dynamic uniforms from ring buffer (per-draw, so matrices are captured at draw time)
        uint32_t vsOffset, psOffset;
        if (!VkState_AllocDynamicUniforms(&vsOffset, &psOffset)) {
            continue;  // Failed to allocate uniform buffer space
        }

        // Check if this stage is multitextured
        qboolean isMultitextured = (qboolean)(pStage->bundle[1].image[0] != NULL);

        // Get the stage-specific variables (texcoords and colors)
        const stageVars_t* stageVars = &input->svars[stage];

        // Allocate and upload vertex data for this stage
        size_t vertSize = input->numVertexes * sizeof(vkGenericVertex_t);
        vkBufferAlloc_t vertAlloc = VkBuffers_AllocVertex(vertSize);
        if (!vertAlloc.data) {
            continue;
        }

        vkGenericVertex_t* verts = (vkGenericVertex_t*)vertAlloc.data;
        for (int i = 0; i < input->numVertexes; i++) {
            // Position (same for all stages)
            verts[i].position[0] = input->xyz[i][0];
            verts[i].position[1] = input->xyz[i][1];
            verts[i].position[2] = input->xyz[i][2];
            verts[i].position[3] = 1.0f;

            // Texcoords from this stage's svars
            verts[i].texCoord0[0] = stageVars->texcoords[0][i][0];
            verts[i].texCoord0[1] = stageVars->texcoords[0][i][1];

            // Second texcoord set (for multitexture/lightmap)
            verts[i].texCoord1[0] = stageVars->texcoords[1][i][0];
            verts[i].texCoord1[1] = stageVars->texcoords[1][i][1];

            // Vertex color from this stage's svars
            verts[i].color[0] = stageVars->colors[i][0];
            verts[i].color[1] = stageVars->colors[i][1];
            verts[i].color[2] = stageVars->colors[i][2];
            verts[i].color[3] = stageVars->colors[i][3];
        }

        // Bind vertex/index buffers
        VkDeviceSize offsets[] = { vertAlloc.offset };
        qvkCmdBindVertexBuffers(frame->commandBuffer, 0, 1, &vertAlloc.buffer, offsets);
        qvkCmdBindIndexBuffer(frame->commandBuffer, idxAlloc.buffer, idxAlloc.offset, VK_INDEX_TYPE_UINT16);

        // Get pipeline for this stage's state
        VkPipeline pipeline = VkState_GetPipeline(
            pStage->stateBits,
            input->shader ? input->shader->cullType : CT_TWO_SIDED,
            backEnd.viewParms.isMirror,
            isMultitextured,
            qfalse
        );

        if (!pipeline) continue;

        qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind textures for this stage
        if (pStage->bundle[0].image[0]) {
            VkDescriptorSet texSet;

            if (isMultitextured && pStage->bundle[1].image[0]) {
                // For multi-texture stages, allocate a fresh descriptor set from the frame's pool
                // This avoids updating descriptor sets that may already be bound to the command buffer
                texSet = VkImage_AllocMultitextureSet(pStage->bundle[0].image[0], pStage->bundle[1].image[0]);
            } else {
                // For single-texture stages, use the image's static descriptor set
                texSet = VkImage_GetDescriptorSet(pStage->bundle[0].image[0]);
            }

            if (texSet) {
                // Bind with dynamic offsets for the uniform buffers
                uint32_t dynamicOffsets[2] = { vsOffset, psOffset };
                qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    VkState_GetPipelineLayout(), 0, 1, &texSet, 2, dynamicOffsets);
            }
        }

        // Draw
        qvkCmdDrawIndexed(frame->commandBuffer, input->numIndexes, 1, 0, 0, 0);

        // Allow skipping to show just lightmaps (like D3D11)
        if (r_lightmap->integer && (pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap)) {
            break;
        }
    }

    // Fog pass - blends fog color over the scene (matches D3D11 TessDrawFog)
    if (input->fogNum && input->shader->fogPass) {
        // Set fog blend state
        unsigned long fogStateBits;
        if (input->shader->fogPass == FP_EQUAL) {
            fogStateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL;
        } else {
            fogStateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        }
        VkState_SetState(fogStateBits);

        // Allocate dynamic uniforms for fog pass
        uint32_t fogVsOffset, fogPsOffset;
        if (VkState_AllocDynamicUniforms(&fogVsOffset, &fogPsOffset)) {
            // Allocate and upload fog vertex data
            size_t fogVertSize = input->numVertexes * sizeof(vkGenericVertex_t);
            vkBufferAlloc_t fogVertAlloc = VkBuffers_AllocVertex(fogVertSize);
            if (fogVertAlloc.data) {
                vkGenericVertex_t* fogVerts = (vkGenericVertex_t*)fogVertAlloc.data;
                for (int i = 0; i < input->numVertexes; i++) {
                    // Position (same as main pass)
                    fogVerts[i].position[0] = input->xyz[i][0];
                    fogVerts[i].position[1] = input->xyz[i][1];
                    fogVerts[i].position[2] = input->xyz[i][2];
                    fogVerts[i].position[3] = 1.0f;

                    // Fog texcoords
                    fogVerts[i].texCoord0[0] = input->fogVars.texcoords[0][i][0];
                    fogVerts[i].texCoord0[1] = input->fogVars.texcoords[0][i][1];
                    fogVerts[i].texCoord1[0] = 0.0f;
                    fogVerts[i].texCoord1[1] = 0.0f;

                    // Fog colors
                    fogVerts[i].color[0] = input->fogVars.colors[i][0];
                    fogVerts[i].color[1] = input->fogVars.colors[i][1];
                    fogVerts[i].color[2] = input->fogVars.colors[i][2];
                    fogVerts[i].color[3] = input->fogVars.colors[i][3];
                }

                // Bind fog vertex buffer
                VkDeviceSize fogOffsets[] = { fogVertAlloc.offset };
                qvkCmdBindVertexBuffers(frame->commandBuffer, 0, 1, &fogVertAlloc.buffer, fogOffsets);

                // Get fog pipeline (single texture, no multitexture)
                VkPipeline fogPipeline = VkState_GetPipeline(
                    fogStateBits,
                    input->shader->cullType,
                    backEnd.viewParms.isMirror,
                    qfalse,  // not multitextured
                    qfalse
                );

                if (fogPipeline) {
                    qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fogPipeline);

                    // Bind fog texture (tr.fogImage)
                    VkDescriptorSet fogTexSet = VkImage_GetDescriptorSet(tr.fogImage);
                    if (fogTexSet) {
                        uint32_t fogDynamicOffsets[2] = { fogVsOffset, fogPsOffset };
                        qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            VkState_GetPipelineLayout(), 0, 1, &fogTexSet, 2, fogDynamicOffsets);
                    }

                    // Draw fog
                    qvkCmdDrawIndexed(frame->commandBuffer, input->numIndexes, 1, 0, 0, 0);
                }
            }
        }
    }
}

void VkDraw_StageVertexLitTexture(const shaderCommands_t* input)
{
    // Use generic for now
    VkDraw_StageGeneric(input);
}

void VkDraw_StageLightmappedMultitexture(const shaderCommands_t* input)
{
    // Use generic for now
    VkDraw_StageGeneric(input);
}

//=============================================================================
// Tessellation
//=============================================================================

void VkDraw_BeginTessellate(const shaderCommands_t* input)
{
    // Called before tessellation - ensure frame is started
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }
}

void VkDraw_EndTessellate(const shaderCommands_t* input)
{
    // Called after tessellation
}

//=============================================================================
// Skybox
//=============================================================================

void VkDraw_SkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint)
{
    if (!skybox || !s_skyboxVertexBuffer) {
        return;
    }

    // Ensure we're in a frame and render pass
    if (!vk.inFrame) {
        VkDraw_BeginFrame();
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];
    if (!frame->inRenderPass) {
        BeginRenderPass(NULL, 1.0f);
    }

    // Set state for skybox (no blend, depth read only - handled by pipeline)
    VkState_SetState(0);

    // Set eye position for skybox centering (skybox follows camera)
    VkState_SetEyePos(eye_origin);

    // Allocate dynamic uniforms from ring buffer (once for all 6 skybox sides)
    uint32_t vsOffset, psOffset;
    if (!VkState_AllocDynamicUniforms(&vsOffset, &psOffset)) {
        return;  // Failed to allocate uniform buffer space
    }

    // Get skybox pipeline
    VkPipeline pipeline = VkState_GetSkyboxPipeline();
    if (!pipeline) {
        return;
    }

    // Set viewport (same as 3D rendering - use negative height for Y flip)
    int vpWidth = g_vkPipelineState.viewportWidth;
    int vpHeight = g_vkPipelineState.viewportHeight;
    if (vpWidth <= 0 || vpHeight <= 0) {
        vpWidth = vk.swapchain.extent.width;
        vpHeight = vk.swapchain.extent.height;
    }

    VkViewport viewport = {};
    viewport.x = (float)g_vkPipelineState.viewportX;
    viewport.y = (float)(vk.swapchain.extent.height - g_vkPipelineState.viewportY);
    viewport.width = (float)vpWidth;
    viewport.height = -(float)vpHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    qvkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = vk.swapchain.extent.width;
    scissor.extent.height = vk.swapchain.extent.height;
    qvkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

    // Bind pipeline
    qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind skybox vertex buffer
    VkDeviceSize offset = 0;
    qvkCmdBindVertexBuffers(frame->commandBuffer, 0, 1, &s_skyboxVertexBuffer, &offset);

    // Draw each side of the skybox
    for (int i = 0; i < 6; i++) {
        const skyboxSideDrawInfo_t* side = &skybox->sides[i];

        if (!side->image) {
            continue;
        }

        // Get descriptor set for this side's texture
        VkDescriptorSet texSet = VkImage_GetDescriptorSet(side->image);
        if (!texSet) {
            continue;
        }

        // Bind texture with dynamic UBO offsets
        uint32_t dynamicOffsets[2] = { vsOffset, psOffset };
        qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            VkState_GetPipelineLayout(), 0, 1, &texSet, 2, dynamicOffsets);

        // Draw 6 vertices (2 triangles) for this side
        // Vertices are arranged: side 0 = verts 0-5, side 1 = verts 6-11, etc.
        qvkCmdDraw(frame->commandBuffer, 6, 1, i * 6, 0);
    }
}

//=============================================================================
// Beam
//=============================================================================

void VkDraw_Beam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs)
{
    // TODO: Implement beam rendering
}

//=============================================================================
// Shadows
//=============================================================================

void VkDraw_ShadowSilhouette(const float* edges, int edgeCount)
{
    // TODO: Implement shadow volume silhouette
}

void VkDraw_ShadowFinish(void)
{
    // TODO: Implement shadow volume finish
}
