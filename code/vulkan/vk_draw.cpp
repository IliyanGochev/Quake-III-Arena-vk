// Vulkan rendering backend - draw commands implementation

#include "vk_draw.h"
#include "vk_state.h"
#include "vk_image.h"
#include "vk_buffers.h"
#include "vk_shaders.h"
#include <string.h>

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

//=============================================================================
// Initialization
//=============================================================================

void VkDraw_Init(void)
{
    // Additional draw-specific initialization if needed
}

void VkDraw_Shutdown(void)
{
    // Cleanup
}

//=============================================================================
// Frame management
//=============================================================================

// Debug counter for draw calls per frame
static int s_drawCallCount = 0;

qboolean VkDraw_BeginFrame(void)
{
    if (vk.inFrame) {
        return qtrue;
    }

    // Reset debug counter
    s_drawCallCount = 0;

    vkFrame_t* frame = &vk.frames[vk.currentFrame];

    // Wait for this frame's fence
    qvkWaitForFences(vk.device, 1, &frame->inFlightFence, VK_TRUE, UINT64_MAX);
    qvkResetFences(vk.device, 1, &frame->inFlightFence);

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
        // Debug: use magenta to see if render pass is working
        clearValues[0].color.float32[0] = 1.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 1.0f;
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

    // Set default viewport and scissor
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
    // In Vulkan, clear happens at render pass begin or via clear attachments
    // For now, just start the render pass with clear values
    BeginRenderPass(clearCol, depth);
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
        qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind vertex/index buffers
        VkDeviceSize offsets[] = { vertAlloc.offset };
        qvkCmdBindVertexBuffers(frame->commandBuffer, 0, 1, &vertAlloc.buffer, offsets);
        qvkCmdBindIndexBuffer(frame->commandBuffer, idxAlloc.buffer, idxAlloc.offset, VK_INDEX_TYPE_UINT16);

        // Bind texture descriptor set (set 0 contains UBOs and texture bindings)
        qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            VkState_GetPipelineLayout(), 0, 1, &texSet, 0, NULL);

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

    // Debug: print first few draw calls per frame
    s_drawCallCount++;
    if (s_drawCallCount <= 3) {
        shaderStage_t* pStage = input->xstages[0];
        Com_Printf("VkDraw_StageGeneric: verts=%d, idxs=%d, stateBits=0x%x, image=%p\n",
            input->numVertexes, input->numIndexes,
            pStage ? pStage->stateBits : 0,
            pStage ? (void*)pStage->bundle[0].image[0] : NULL);

        // Print first few vertex positions
        if (input->numVertexes > 0) {
            Com_Printf("  Vertex[0]: xyz=(%.2f, %.2f, %.2f)\n",
                input->xyz[0][0], input->xyz[0][1], input->xyz[0][2]);
            if (input->numVertexes > 1) {
                Com_Printf("  Vertex[1]: xyz=(%.2f, %.2f, %.2f)\n",
                    input->xyz[1][0], input->xyz[1][1], input->xyz[1][2]);
            }
        }
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
    // Convert OpenGL viewport Y (bottom-up) to Vulkan (top-down)
    viewport.y = (float)(vk.swapchain.extent.height - g_vkPipelineState.viewportY - vpHeight);
    viewport.width = (float)vpWidth;
    viewport.height = (float)vpHeight;
    viewport.minDepth = g_vkPipelineState.depthMin;
    viewport.maxDepth = g_vkPipelineState.depthMax;
    qvkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = g_vkPipelineState.viewportX;
    // Convert scissor Y to Vulkan coordinates
    scissor.offset.y = vk.swapchain.extent.height - g_vkPipelineState.viewportY - vpHeight;
    scissor.extent.width = vpWidth;
    scissor.extent.height = vpHeight;
    qvkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

    // Update uniforms before drawing
    VkState_UpdateUniforms();

    // Iterate through shader stages using xstages (the active stage list)
    for (int stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        shaderStage_t* pStage = input->xstages[stage];
        if (!pStage) {
            break;
        }

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
            (qboolean)(pStage->bundle[1].image[0] != NULL), // multitextured if has second texture
            qfalse
        );

        if (!pipeline) continue;

        qvkCmdBindPipeline(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind texture for this stage (set 0 contains UBOs and texture bindings)
        if (pStage->bundle[0].image[0]) {
            VkDescriptorSet texSet = VkImage_GetDescriptorSet(pStage->bundle[0].image[0]);

            // DEBUG: Print descriptor set being bound
            static int bindDebugCount = 0;
            if (bindDebugCount < 5) {
                bindDebugCount++;
                Com_Printf("Binding descriptor set: %p for image %p\n",
                    (void*)texSet, (void*)pStage->bundle[0].image[0]);
            }

            if (texSet) {
                qvkCmdBindDescriptorSets(frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    VkState_GetPipelineLayout(), 0, 1, &texSet, 0, NULL);
            }
        }

        // Draw
        qvkCmdDrawIndexed(frame->commandBuffer, input->numIndexes, 1, 0, 0, 0);
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
    // TODO: Implement skybox rendering
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
