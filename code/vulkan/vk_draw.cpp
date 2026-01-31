#include "vk_common.h"
#include "vk_device.h"
#include "vk_draw.h"
#include "vk_state.h"
#include "vk_image.h"

//----------------------------------------------------------------------------
// Global draw state
//----------------------------------------------------------------------------
vkDrawState_t g_vkDraw;

//----------------------------------------------------------------------------
// Create circular buffer
//----------------------------------------------------------------------------
void VK_CreateCircularBuffer(vkCircularBuffer_t* buf, uint32_t size, VkBufferUsageFlags usage) {
    buf->size = size;
    buf->usage = usage;  // Store usage for alignment checks

    // Create buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocResInfo;
    VK_CHECK(vmaCreateBuffer(g_vkDevice.allocator, &bufferInfo, &allocInfo,
                            &buf->buffer, &buf->allocation, &allocResInfo));

    buf->mappedData = allocResInfo.pMappedData;

    // Initialize offsets
    buf->currentOffset = 0;
    buf->nextOffset = 0;
}

//----------------------------------------------------------------------------
// Destroy circular buffer
//----------------------------------------------------------------------------
void VK_DestroyCircularBuffer(vkCircularBuffer_t* buf) {
    if (buf->buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(g_vkDevice.allocator, buf->buffer, buf->allocation);
        buf->buffer = VK_NULL_HANDLE;
        buf->mappedData = nullptr;
    }
}

//----------------------------------------------------------------------------
// Reset circular buffer for new frame
//----------------------------------------------------------------------------
void VK_ResetCircularBuffer(vkCircularBuffer_t* buf, uint32_t frameIndex) {
    // Reset to beginning - fence ensures previous use is complete
    buf->currentOffset = 0;
    buf->nextOffset = 0;
}

//----------------------------------------------------------------------------
// Update circular buffer (allocate space and copy data)
//----------------------------------------------------------------------------
void VK_UpdateCircularBuffer(vkCircularBuffer_t* buf, const void* data, uint32_t dataSize) {
    // Determine alignment based on buffer usage
    // Uniform buffers require 256-byte alignment per Vulkan spec
    uint32_t alignment = (buf->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) ? 256 : 16;

    // Align offsets
    uint32_t alignedNextOffset = (buf->nextOffset + alignment - 1) & ~(alignment - 1);
    uint32_t alignedSize = (dataSize + alignment - 1) & ~(alignment - 1);

    // Simple wrap condition (like D3D11)
    if (alignedNextOffset + alignedSize > buf->size) {
        // Wrap to start of buffer
        buf->currentOffset = 0;
        buf->nextOffset = alignedSize;
    } else {
        buf->currentOffset = alignedNextOffset;
        buf->nextOffset = alignedNextOffset + alignedSize;
    }

    // Copy data
    if (data && buf->mappedData) {
        memcpy((byte*)buf->mappedData + buf->currentOffset, data, dataSize);
    }
}

//----------------------------------------------------------------------------
// Create uniform buffers
//----------------------------------------------------------------------------
void VK_CreateUniformBuffers() {
    // View VS uniform (per-frame)
    VK_CreateCircularBuffer(&g_vkDraw.uniformBuffers.viewVS,
                           sizeof(vkViewVSUniform_t) * 256,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // View PS uniform (per-frame)
    VK_CreateCircularBuffer(&g_vkDraw.uniformBuffers.viewPS,
                           sizeof(vkViewPSUniform_t) * 256,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Stage uniform (per-stage)
    VK_CreateCircularBuffer(&g_vkDraw.uniformBuffers.stage,
                           sizeof(vkStageUniform_t) * 1024,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Skybox VS uniform
    VK_CreateCircularBuffer(&g_vkDraw.uniformBuffers.skyboxVS,
                           sizeof(vkSkyboxVSUniform_t) * 64,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Skybox PS uniform
    VK_CreateCircularBuffer(&g_vkDraw.uniformBuffers.skyboxPS,
                           sizeof(vkSkyboxPSUniform_t) * 64,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

//----------------------------------------------------------------------------
// Destroy uniform buffers
//----------------------------------------------------------------------------
void VK_DestroyUniformBuffers() {
    VK_DestroyCircularBuffer(&g_vkDraw.uniformBuffers.viewVS);
    VK_DestroyCircularBuffer(&g_vkDraw.uniformBuffers.viewPS);
    VK_DestroyCircularBuffer(&g_vkDraw.uniformBuffers.stage);
    VK_DestroyCircularBuffer(&g_vkDraw.uniformBuffers.skyboxVS);
    VK_DestroyCircularBuffer(&g_vkDraw.uniformBuffers.skyboxPS);
}

//----------------------------------------------------------------------------
// Create tessellation buffers
//----------------------------------------------------------------------------
void VK_CreateTessellationBuffers() {
    // Index buffer (2 MB)
    VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.indexes,
                           2 * 1024 * 1024,
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // Position buffer (4 MB - vec4_t per vertex)
    VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.xyz,
                           4 * 1024 * 1024,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Stage buffers (per shader stage)
    for (int i = 0; i < MAX_SHADER_STAGES; i++) {
        // Texture coordinate buffers (per bundle)
        for (int j = 0; j < NUM_TEXTURE_BUNDLES; j++) {
            VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.stages[i].texCoords[j],
                                   1 * 1024 * 1024,  // 1 MB per texcoord buffer
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }

        // Color buffer
        VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.stages[i].colors,
                               512 * 1024,  // 512 KB
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    // Dynamic light buffers
    for (int i = 0; i < MAX_DLIGHTS; i++) {
        VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].indexes,
                               256 * 1024,
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].texCoords,
                               256 * 1024,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].colors,
                               256 * 1024,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    // Fog buffers
    VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.fog.texCoords,
                           256 * 1024,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VK_CreateCircularBuffer(&g_vkDraw.tessBuffers.fog.colors,
                           256 * 1024,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

//----------------------------------------------------------------------------
// Destroy tessellation buffers
//----------------------------------------------------------------------------
void VK_DestroyTessellationBuffers() {
    // Index and position buffers
    VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.indexes);
    VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.xyz);

    // Stage buffers
    for (int i = 0; i < MAX_SHADER_STAGES; i++) {
        for (int j = 0; j < NUM_TEXTURE_BUNDLES; j++) {
            VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.stages[i].texCoords[j]);
        }
        VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.stages[i].colors);
    }

    // Dynamic light buffers
    for (int i = 0; i < MAX_DLIGHTS; i++) {
        VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].indexes);
        VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].texCoords);
        VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.dlights[i].colors);
    }

    // Fog buffers
    VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.fog.texCoords);
    VK_DestroyCircularBuffer(&g_vkDraw.tessBuffers.fog.colors);
}

//----------------------------------------------------------------------------
// Create descriptor sets
//----------------------------------------------------------------------------
void VK_CreateDescriptorSets() {
    // Allocate view descriptor sets (one per frame)
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_vkPipelines.descriptorPool;
    allocInfo.descriptorSetCount = VK_MAX_FRAMES_IN_FLIGHT;

    VkDescriptorSetLayout layouts[VK_MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = g_vkPipelines.setLayouts[0];  // Set 0: View
    }
    allocInfo.pSetLayouts = layouts;

    VK_CHECK(vkAllocateDescriptorSets(g_vkDevice.device, &allocInfo, g_vkDraw.descriptorSets.viewSets));

    // Allocate stage descriptor sets (one per frame)
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = g_vkPipelines.setLayouts[1];  // Set 1: Stage/Material
    }
    VK_CHECK(vkAllocateDescriptorSets(g_vkDevice.device, &allocInfo, g_vkDraw.descriptorSets.stageSets));
}

//----------------------------------------------------------------------------
// Update view descriptor set
//----------------------------------------------------------------------------
void VK_UpdateViewDescriptorSet(uint32_t frameIndex) {
    VkDescriptorBufferInfo bufferInfoVS = {};
    bufferInfoVS.buffer = g_vkDraw.uniformBuffers.viewVS.buffer;
    bufferInfoVS.offset = g_vkDraw.uniformBuffers.viewVS.currentOffset;
    bufferInfoVS.range = sizeof(vkViewVSUniform_t);

    VkDescriptorBufferInfo bufferInfoPS = {};
    bufferInfoPS.buffer = g_vkDraw.uniformBuffers.viewPS.buffer;
    bufferInfoPS.offset = g_vkDraw.uniformBuffers.viewPS.currentOffset;
    bufferInfoPS.range = sizeof(vkViewPSUniform_t);

    VkWriteDescriptorSet descriptorWrites[2] = {};

    // Binding 0: ViewVS uniform
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = g_vkDraw.descriptorSets.viewSets[frameIndex];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfoVS;

    // Binding 1: ViewPS uniform
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = g_vkDraw.descriptorSets.viewSets[frameIndex];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfoPS;

    vkUpdateDescriptorSets(g_vkDevice.device, 2, descriptorWrites, 0, nullptr);

    // Store current view set for binding
    g_vkDraw.descriptorSets.currentViewSet = g_vkDraw.descriptorSets.viewSets[frameIndex];
}

//----------------------------------------------------------------------------
// Update stage descriptor set
//----------------------------------------------------------------------------
void VK_UpdateStageDescriptorSet(uint32_t frameIndex) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = g_vkDraw.uniformBuffers.stage.buffer;
    bufferInfo.offset = g_vkDraw.uniformBuffers.stage.currentOffset;
    bufferInfo.range = sizeof(vkStageUniform_t);

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = g_vkDraw.descriptorSets.stageSets[frameIndex];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(g_vkDevice.device, 1, &descriptorWrite, 0, nullptr);

    // Store current stage set for binding
    g_vkDraw.descriptorSets.currentStageSet = g_vkDraw.descriptorSets.stageSets[frameIndex];
}

//----------------------------------------------------------------------------
// Update view VS uniform
//----------------------------------------------------------------------------
void VK_UpdateViewVSUniform() {
    vkViewVSUniform_t uniform;

    // Copy matrices from run state
    memcpy(uniform.projectionMatrix, g_vkRunState.projectionMatrix, sizeof(uniform.projectionMatrix));
    memcpy(uniform.modelViewMatrix, g_vkRunState.modelViewMatrix, sizeof(uniform.modelViewMatrix));

    // Depth range
    uniform.depthRange[0] = g_vkRunState.depthRange[0];
    uniform.depthRange[1] = g_vkRunState.depthRange[1];

    // Padding
    uniform.padding[0] = 0.0f;
    uniform.padding[1] = 0.0f;

    // Upload to circular buffer
    VK_UpdateCircularBuffer(&g_vkDraw.uniformBuffers.viewVS, &uniform, sizeof(uniform));

    // Mark as clean
    g_vkRunState.viewVSDirty = qfalse;
}

//----------------------------------------------------------------------------
// Update view PS uniform
//----------------------------------------------------------------------------
void VK_UpdateViewPSUniform() {
    vkViewPSUniform_t uniform;

    // Clip plane
    uniform.clipPlane[0] = g_vkRunState.clipPlane[0];
    uniform.clipPlane[1] = g_vkRunState.clipPlane[1];
    uniform.clipPlane[2] = g_vkRunState.clipPlane[2];
    uniform.clipPlane[3] = g_vkRunState.clipPlane[3];

    // Alpha clip
    uniform.alphaClip[0] = g_vkRunState.alphaClip[0];
    uniform.alphaClip[1] = g_vkRunState.alphaClip[1];

    // Padding
    uniform.padding[0] = 0.0f;
    uniform.padding[1] = 0.0f;

    // Upload to circular buffer
    VK_UpdateCircularBuffer(&g_vkDraw.uniformBuffers.viewPS, &uniform, sizeof(uniform));

    // Mark as clean
    g_vkRunState.viewPSDirty = qfalse;
}

//----------------------------------------------------------------------------
// Create frame resources
//----------------------------------------------------------------------------
void VK_CreateFrameResources() {
    // Zero out state
    memset(&g_vkDraw, 0, sizeof(g_vkDraw));

    // Create uniform buffers
    VK_CreateUniformBuffers();

    // Create tessellation buffers
    VK_CreateTessellationBuffers();

    // Create descriptor sets
    VK_CreateDescriptorSets();
}

//----------------------------------------------------------------------------
// Destroy frame resources
//----------------------------------------------------------------------------
void VK_DestroyFrameResources() {
    // Wait for device idle
    vkDeviceWaitIdle(g_vkDevice.device);

    // Destroy tessellation buffers
    VK_DestroyTessellationBuffers();

    // Destroy uniform buffers
    VK_DestroyUniformBuffers();

    // Descriptor sets are freed by pool destruction
}

//----------------------------------------------------------------------------
// Get current command buffer
//----------------------------------------------------------------------------
VkCommandBuffer VK_GetCurrentCommandBuffer() {
    return g_vkDevice.frames[g_vkDraw.currentFrame].commandBuffer;
}

//----------------------------------------------------------------------------
// Begin frame
//----------------------------------------------------------------------------
void VK_BeginFrame() {
    vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];

    // Wait for previous frame to finish
    VK_CHECK(vkWaitForFences(g_vkDevice.device, 1, &frame->renderFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(g_vkDevice.device, 1, &frame->renderFence));

    // Acquire swapchain image
    VkResult result = vkAcquireNextImageKHR(g_vkDevice.device, g_vkDevice.swapchain, UINT64_MAX,
                                            frame->imageAvailable, VK_NULL_HANDLE, &g_vkDraw.imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: Handle swapchain recreation
        ri.Printf(PRINT_WARNING, "WARNING: Swapchain out of date, need recreation\n");
    } else if (result != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "Failed to acquire swapchain image: 0x%08X\n", result);
    }

    // Upload pending dynamic images (cinematics) BEFORE starting render pass
    // This ensures texture updates don't interrupt the render pass mid-frame
    VK_UploadPendingImages();

    // Reset and begin command buffer
    VK_CHECK(vkResetCommandBuffer(frame->commandBuffer, 0));
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(frame->commandBuffer, &beginInfo));

    // Reset circular buffers for this frame
    VK_ResetCircularBuffer(&g_vkDraw.uniformBuffers.viewVS, g_vkDraw.currentFrame);
    VK_ResetCircularBuffer(&g_vkDraw.uniformBuffers.viewPS, g_vkDraw.currentFrame);
    VK_ResetCircularBuffer(&g_vkDraw.uniformBuffers.stage, g_vkDraw.currentFrame);

    VK_ResetCircularBuffer(&g_vkDraw.tessBuffers.indexes, g_vkDraw.currentFrame);
    VK_ResetCircularBuffer(&g_vkDraw.tessBuffers.xyz, g_vkDraw.currentFrame);

    for (int i = 0; i < MAX_SHADER_STAGES; i++) {
        for (int j = 0; j < NUM_TEXTURE_BUNDLES; j++) {
            VK_ResetCircularBuffer(&g_vkDraw.tessBuffers.stages[i].texCoords[j], g_vkDraw.currentFrame);
        }
        VK_ResetCircularBuffer(&g_vkDraw.tessBuffers.stages[i].colors, g_vkDraw.currentFrame);
    }

    // Begin render pass
    qboolean msaaEnabled = (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? qtrue : qfalse;

    VkClearValue clearValues[3];
    // Clear with alpha = 1.0 to keep window opaque (swapchain has alpha channel)
    // Color write mask excludes alpha, so it will stay at 1.0 throughout the frame

    if (msaaEnabled) {
        // MSAA: 3 attachments (MSAA color, resolve, depth)
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};     // MSAA color buffer
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};     // Resolve target (swapchain)
        clearValues[2].depthStencil = {1.0f, 0};                // Depth/stencil
    } else {
        // Non-MSAA: 2 attachments (color, depth)
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};     // Color buffer (swapchain)
        clearValues[1].depthStencil = {1.0f, 0};                // Depth/stencil (CRITICAL FIX!)
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_vkDevice.renderPass;
    renderPassInfo.framebuffer = g_vkDevice.framebuffers[g_vkDraw.imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_vkDevice.swapchainExtent;
    renderPassInfo.clearValueCount = msaaEnabled ? 3 : 2;  // 3 for MSAA, 2 for non-MSAA
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(frame->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic viewport with negative height for Y-flip
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = (float)g_vkDevice.swapchainExtent.height;  // Bottom edge
    viewport.width = (float)g_vkDevice.swapchainExtent.width;
    viewport.height = -(float)g_vkDevice.swapchainExtent.height;  // Negative to flip Y
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    static int beginFrameLogCount = 0;
    if (beginFrameLogCount < 3) {
        ri.Printf(PRINT_ALL, "VK_BeginFrame: setting viewport to swapchainExtent %dx%d (vp: x=%.0f, y=%.0f, w=%.0f, h=%.0f)\n",
                  g_vkDevice.swapchainExtent.width, g_vkDevice.swapchainExtent.height,
                  viewport.x, viewport.y, viewport.width, viewport.height);
        beginFrameLogCount++;
    }

    vkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

    // Set dynamic scissor
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = g_vkDevice.swapchainExtent;

    vkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

    // Set dynamic depth bias (declared as dynamic state in pipeline)
    // Since we're not using polygon offset, set all values to 0
    vkCmdSetDepthBias(frame->commandBuffer, 0.0f, 0.0f, 0.0f);

    g_vkDraw.inRenderPass = qtrue;
}

//----------------------------------------------------------------------------
// End frame
//----------------------------------------------------------------------------
extern "C" void VK_EndFrame() {
    // If no frame was started (nothing was rendered), start one now to maintain frame pacing
    if (!g_vkDraw.inRenderPass) {
        VK_BeginFrame();
    }

    vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];

    // End render pass
    if (g_vkDraw.inRenderPass) {
        vkCmdEndRenderPass(frame->commandBuffer);
        g_vkDraw.inRenderPass = qfalse;
    }

    // End command buffer
    VK_CHECK(vkEndCommandBuffer(frame->commandBuffer));

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame->imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame->renderFinished; 

    VkResult submitResult = vkQueueSubmit(g_vkDevice.graphicsQueue, 1, &submitInfo, frame->renderFence);
    if (submitResult != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "vkQueueSubmit failed with error 0x%08X\n", submitResult);
    }

    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame->renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &g_vkDevice.swapchain;
    presentInfo.pImageIndices = &g_vkDraw.imageIndex;

    VkResult result = vkQueuePresentKHR(g_vkDevice.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: Handle swapchain recreation
        ri.Printf(PRINT_WARNING, "WARNING: Swapchain out of date during present\n");
    } else if (result != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "Failed to present swapchain image: 0x%08X\n", result);
    }

    // Advance to next frame
    g_vkDraw.currentFrame = (g_vkDraw.currentFrame + 1) % VK_MAX_FRAMES_IN_FLIGHT;
}

//----------------------------------------------------------------------------
// Flush GPU (wait for completion)
//----------------------------------------------------------------------------
void VK_FlushGPU() {
    vkDeviceWaitIdle(g_vkDevice.device);
}

//----------------------------------------------------------------------------
// Draw indexed primitives
//----------------------------------------------------------------------------
void VK_DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();
    vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, vertexOffset, 0);
}

//----------------------------------------------------------------------------
// Set vertex buffers for single-texture stage
//----------------------------------------------------------------------------
void VK_SetVertexBuffersST(const vkTessStageBuffers_t* stage) {
    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Binding 0: Position (already bound at tessellation level)
    // Binding 1: TexCoord
    VkBuffer texCoordBuffer = stage->texCoords[0].buffer;
    VkDeviceSize texCoordOffset = stage->texCoords[0].currentOffset;
    vkCmdBindVertexBuffers(cmd, 1, 1, &texCoordBuffer, &texCoordOffset);

    // Binding 2: Color
    VkBuffer colorBuffer = stage->colors.buffer;
    VkDeviceSize colorOffset = stage->colors.currentOffset;
    vkCmdBindVertexBuffers(cmd, 2, 1, &colorBuffer, &colorOffset);
}

//----------------------------------------------------------------------------
// Set vertex buffers for multi-texture stage
//----------------------------------------------------------------------------
void VK_SetVertexBuffersMT(const vkTessStageBuffers_t* stage) {
    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Binding 0: Position (already bound at tessellation level)
    // Binding 1: TexCoord0
    // Binding 2: TexCoord1
    VkBuffer texCoordBuffers[2] = {
        stage->texCoords[0].buffer,
        stage->texCoords[1].buffer
    };
    VkDeviceSize texCoordOffsets[2] = {
        stage->texCoords[0].currentOffset,
        stage->texCoords[1].currentOffset
    };
    vkCmdBindVertexBuffers(cmd, 1, 2, texCoordBuffers, texCoordOffsets);

    // Binding 3: Color
    VkBuffer colorBuffer = stage->colors.buffer;
    VkDeviceSize colorOffset = stage->colors.currentOffset;
    vkCmdBindVertexBuffers(cmd, 3, 1, &colorBuffer, &colorOffset);
}

//----------------------------------------------------------------------------
// Update tessellation buffers
//----------------------------------------------------------------------------
void VK_UpdateTessBuffers(const shaderCommands_t* input, qboolean needDlights, qboolean needFog) {
    // Update index buffer
    uint32_t indexSize = sizeof(glIndex_t) * input->numIndexes;
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.indexes, input->indexes, indexSize);

    // Update position buffer
    uint32_t vertexSize = sizeof(vec4_t) * input->numVertexes;
    VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.xyz, input->xyz, vertexSize);

    // Update per-stage buffers
    for (int stage = 0; stage < MAX_SHADER_STAGES; stage++) {
        if (!input->xstages[stage]) break;

        vkTessStageBuffers_t* stageBuffers = &g_vkDraw.tessBuffers.stages[stage];

        // Colors
        uint32_t colorSize = sizeof(color4ub_t) * input->numVertexes;
        VK_UpdateCircularBuffer(&stageBuffers->colors, input->svars[stage].colors, colorSize);

        // Texture coordinates (bundle 0)
        uint32_t texCoordSize = sizeof(vec2_t) * input->numVertexes;
        VK_UpdateCircularBuffer(&stageBuffers->texCoords[0], input->svars[stage].texcoords[0], texCoordSize);

        // Texture coordinates (bundle 1 - for lightmaps)
        if (input->xstages[stage]->bundle[1].image[0]) {
            VK_UpdateCircularBuffer(&stageBuffers->texCoords[1], input->svars[stage].texcoords[1], texCoordSize);
        }
    }

    // Update dynamic light buffers if needed
    if (needDlights) {
        for (int l = 0; l < input->dlightCount; ++l) {
            const dlightProjectionInfo_t* cpuLight = &input->dlightInfo[l];
            vkTessLightProjBuffers_t* gpuLight = &g_vkDraw.tessBuffers.dlights[l];

            if (!cpuLight->numIndexes) {
                continue;
            }

            // Update light index buffer
            uint32_t lightIndexSize = sizeof(glIndex_t) * cpuLight->numIndexes;
            VK_UpdateCircularBuffer(&gpuLight->indexes, cpuLight->hitIndexes, lightIndexSize);

            // Update light color buffer
            uint32_t lightColorSize = sizeof(byte) * 4 * input->numVertexes;
            VK_UpdateCircularBuffer(&gpuLight->colors, cpuLight->colorArray, lightColorSize);

            // Update light texture coordinate buffer
            uint32_t lightTexCoordSize = sizeof(float) * 2 * input->numVertexes;
            VK_UpdateCircularBuffer(&gpuLight->texCoords, cpuLight->texCoordsArray, lightTexCoordSize);
        }
    }

    // Update fog buffers if needed
    if (needFog) {
        uint32_t fogColorSize = sizeof(color4ub_t) * input->numVertexes;
        VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.fog.colors, input->fogVars.colors, fogColorSize);

        uint32_t fogTexCoordSize = sizeof(vec2_t) * input->numVertexes;
        VK_UpdateCircularBuffer(&g_vkDraw.tessBuffers.fog.texCoords, input->fogVars.texcoords, fogTexCoordSize);
    }
}

//----------------------------------------------------------------------------
// Draw dynamic lights
//----------------------------------------------------------------------------
void VK_DrawDynamicLights(const shaderCommands_t* input) {
    if (!input || input->dlightCount == 0) {
        return;
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Get dlight texture
    vkImage_t* dlightTex = VK_GetImage(tr.dlightImage);
    if (!dlightTex || dlightTex->image == VK_NULL_HANDLE || dlightTex->descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    // Build pipeline key for single-texture rendering
    vkPipelineKey_t key = {};
    key.shaderType = VK_SHADER_SINGLE_TEXTURE;
    key.cullMode = input->shader->cullType;
    key.polygonMode = 0;
    key.sampleCount = g_vkDevice.msaaSamples;

    // Bind descriptor set 2 (texture) - dlight texture
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 2, 1,
                           &dlightTex->descriptorSet, 0, nullptr);

    // Draw each dynamic light
    for (int l = 0; l < input->dlightCount; l++) {
        const dlightProjectionInfo_t* dlInfo = &input->dlightInfo[l];
        if (!dlInfo->numIndexes) {
            continue;
        }

        vkTessLightProjBuffers_t* projBuf = &g_vkDraw.tessBuffers.dlights[l];

        // Select blend mode
        if (dlInfo->additive) {
            key.blendSrc = GLS_SRCBLEND_ONE;
            key.blendDst = GLS_DSTBLEND_ONE;
            key.depthFlags = GLS_DEPTHFUNC_EQUAL;
        } else {
            key.blendSrc = GLS_SRCBLEND_DST_COLOR;
            key.blendDst = GLS_DSTBLEND_ONE;
            key.depthFlags = GLS_DEPTHFUNC_EQUAL;
        }

        // Get or create pipeline with the current blend state
        VkPipeline pipeline = VK_GetOrCreatePipeline(key);
        if (pipeline == VK_NULL_HANDLE) {
            continue;
        }

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Bind light-specific index buffer
        vkCmdBindIndexBuffer(cmd, projBuf->indexes.buffer, projBuf->indexes.currentOffset, VK_INDEX_TYPE_UINT16);

        // Bind light-specific vertex buffers
        // Binding 1: Light texture coordinates
        VkBuffer texCoordBuffer = projBuf->texCoords.buffer;
        VkDeviceSize texCoordOffset = projBuf->texCoords.currentOffset;
        vkCmdBindVertexBuffers(cmd, 1, 1, &texCoordBuffer, &texCoordOffset);

        // Binding 3: Light colors
        VkBuffer colorBuffer = projBuf->colors.buffer;
        VkDeviceSize colorOffset = projBuf->colors.currentOffset;
        vkCmdBindVertexBuffers(cmd, 3, 1, &colorBuffer, &colorOffset);

        // Draw the dynamic light
        vkCmdDrawIndexed(cmd, dlInfo->numIndexes, 1, 0, 0, 0);
    }
}

//----------------------------------------------------------------------------
// Draw fog pass
//----------------------------------------------------------------------------
void VK_DrawFog(const shaderCommands_t* input) {
    if (!input || !input->fogNum || !input->shader->fogPass) {
        return;
    }

    VkCommandBuffer cmd = VK_GetCurrentCommandBuffer();

    // Get fog texture
    vkImage_t* fogTex = VK_GetImage(tr.fogImage);
    if (!fogTex || fogTex->image == VK_NULL_HANDLE || fogTex->descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    // Build pipeline key for single-texture fog rendering
    vkPipelineKey_t key = {};
    key.shaderType = VK_SHADER_SINGLE_TEXTURE;
    key.cullMode = input->shader->cullType;
    key.polygonMode = 0;
    key.sampleCount = g_vkDevice.msaaSamples;

    // Set blend mode for fog
    if (input->shader->fogPass == FP_EQUAL) {
        key.blendSrc = GLS_SRCBLEND_SRC_ALPHA;
        key.blendDst = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        key.depthFlags = GLS_DEPTHFUNC_EQUAL;
    } else {
        key.blendSrc = GLS_SRCBLEND_SRC_ALPHA;
        key.blendDst = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        key.depthFlags = 0;  // Normal depth test
    }

    // Get or create pipeline
    VkPipeline pipeline = VK_GetOrCreatePipeline(key);
    if (pipeline == VK_NULL_HANDLE) {
        return;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind index buffer (use main index buffer)
    vkCmdBindIndexBuffer(cmd, g_vkDraw.tessBuffers.indexes.buffer,
                        g_vkDraw.tessBuffers.indexes.currentOffset, VK_INDEX_TYPE_UINT16);

    // Bind fog vertex buffers
    // Binding 1: Fog texture coordinates
    VkBuffer texCoordBuffer = g_vkDraw.tessBuffers.fog.texCoords.buffer;
    VkDeviceSize texCoordOffset = g_vkDraw.tessBuffers.fog.texCoords.currentOffset;
    vkCmdBindVertexBuffers(cmd, 1, 1, &texCoordBuffer, &texCoordOffset);

    // Binding 3: Fog colors
    VkBuffer colorBuffer = g_vkDraw.tessBuffers.fog.colors.buffer;
    VkDeviceSize colorOffset = g_vkDraw.tessBuffers.fog.colors.currentOffset;
    vkCmdBindVertexBuffers(cmd, 3, 1, &colorBuffer, &colorOffset);

    // Bind descriptor set 2 (texture) - fog texture
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           g_vkPipelines.pipelineLayout, 2, 1,
                           &fogTex->descriptorSet, 0, nullptr);

    // Draw the fog
    vkCmdDrawIndexed(cmd, input->numIndexes, 1, 0, 0, 0);
}
