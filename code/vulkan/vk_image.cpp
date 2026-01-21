#include "vk_common.h"
#include "vk_device.h"
#include "vk_image.h"
#include "vk_state.h"
#include "vk_draw.h"

// External reference to draw state
extern vkDrawState_t g_vkDraw;

//----------------------------------------------------------------------------
// Global image array
//----------------------------------------------------------------------------
vkImage_t g_vkImages[VK_MAX_IMAGES];

//----------------------------------------------------------------------------
// Initialize image system
//----------------------------------------------------------------------------
void VK_InitImages() {
    // Zero out image array
    memset(g_vkImages, 0, sizeof(g_vkImages));
}

//----------------------------------------------------------------------------
// Shutdown image system
//----------------------------------------------------------------------------
void VK_ShutdownImages() {
    // Clean up all images
    for (int i = 0; i < VK_MAX_IMAGES; i++) {
        vkImage_t* vkImg = &g_vkImages[i];

        if (vkImg->image != VK_NULL_HANDLE) {
            // Destroy staging buffer if exists
            if (vkImg->stagingBuffer != VK_NULL_HANDLE) {
                if (vkImg->stagingMappedData != nullptr) {
                    vmaUnmapMemory(g_vkDevice.allocator, vkImg->stagingAllocation);
                }
                vmaDestroyBuffer(g_vkDevice.allocator, vkImg->stagingBuffer, vkImg->stagingAllocation);
            }

            // Destroy sampler
            if (vkImg->sampler != VK_NULL_HANDLE) {
                vkDestroySampler(g_vkDevice.device, vkImg->sampler, nullptr);
            }

            // Destroy image view
            if (vkImg->view != VK_NULL_HANDLE) {
                vkDestroyImageView(g_vkDevice.device, vkImg->view, nullptr);
            }

            // Destroy image
            vmaDestroyImage(g_vkDevice.allocator, vkImg->image, vkImg->allocation);

            // Descriptor set will be freed by pool destruction
        }
    }

    memset(g_vkImages, 0, sizeof(g_vkImages));
}

//----------------------------------------------------------------------------
// Get Vulkan image by engine image index
//----------------------------------------------------------------------------
vkImage_t* VK_GetImage(const image_t* image) {
    if (!image || image->index < 0 || image->index >= VK_MAX_IMAGES) {
        return nullptr;
    }
    return &g_vkImages[image->index];
}

//----------------------------------------------------------------------------
// Begin single-time commands
//----------------------------------------------------------------------------
VkCommandBuffer VK_BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // FIXED: Use dedicated transfer pool instead of frame pool to avoid conflicts
    allocInfo.commandPool = g_vkDevice.transferCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(g_vkDevice.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

//----------------------------------------------------------------------------
// End single-time commands
//----------------------------------------------------------------------------
void VK_EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(g_vkDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vkDevice.graphicsQueue);

    // FIXED: Free from transfer pool instead of frame pool
    vkFreeCommandBuffers(g_vkDevice.device, g_vkDevice.transferCommandPool, 1, &commandBuffer);
}

//----------------------------------------------------------------------------
// Create staging buffer
//----------------------------------------------------------------------------
VkBuffer VK_CreateStagingBuffer(VkDeviceSize size, VmaAllocation* allocation, void** mappedData) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer buffer;
    VmaAllocationInfo allocResInfo;
    VK_CHECK(vmaCreateBuffer(g_vkDevice.allocator, &bufferInfo, &allocInfo, &buffer, allocation, &allocResInfo));

    if (mappedData) {
        *mappedData = allocResInfo.pMappedData;
    }

    return buffer;
}

//----------------------------------------------------------------------------
// Destroy staging buffer
//----------------------------------------------------------------------------
void VK_DestroyStagingBuffer(VkBuffer buffer, VmaAllocation allocation) {
    vmaDestroyBuffer(g_vkDevice.allocator, buffer, allocation);
}

//----------------------------------------------------------------------------
// Create VkImage
//----------------------------------------------------------------------------
VkImage VK_CreateImage2D(uint32_t width, uint32_t height, uint32_t mipLevels,
                         VkSampleCountFlagBits numSamples, VkFormat format,
                         VkImageTiling tiling, VkImageUsageFlags usage,
                         VmaAllocation* allocation) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image;
    VK_CHECK(vmaCreateImage(g_vkDevice.allocator, &imageInfo, &allocInfo, &image, allocation, nullptr));

    return image;
}

//----------------------------------------------------------------------------
// Create image view
//----------------------------------------------------------------------------
VkImageView VK_CreateImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(g_vkDevice.device, &viewInfo, nullptr, &imageView));

    return imageView;
}

//----------------------------------------------------------------------------
// Create sampler
//----------------------------------------------------------------------------
VkSampler VK_CreateSampler(qboolean mipmap, wrapClampMode_t wrapMode) {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // Filtering
    if (mipmap) {
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    } else {
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    // Wrap mode
    VkSamplerAddressMode addressMode;
    if (wrapMode == WRAPMODE_CLAMP) {
        addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    } else {
        addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;

    // Anisotropy
    cvar_t* r_ext_texture_filter_anisotropic = ri.Cvar_Get("r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE);
    if (r_ext_texture_filter_anisotropic->integer) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
    } else {
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = mipmap ? VK_LOD_CLAMP_NONE : 0.0f;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(g_vkDevice.device, &samplerInfo, nullptr, &sampler));

    return sampler;
}

//----------------------------------------------------------------------------
// Transition image layout
//----------------------------------------------------------------------------
void VK_TransitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels) {
    VkCommandBuffer commandBuffer = VK_BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // Initial transition for uploading texture data
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // After upload, transition to shader read
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // For updating dynamic images (cinematics)
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        ri.Error(ERR_FATAL, "Unsupported layout transition: %d -> %d\n", oldLayout, newLayout);
        return;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    VK_EndSingleTimeCommands(commandBuffer);
}

//----------------------------------------------------------------------------
// Copy buffer to image
//----------------------------------------------------------------------------
void VK_CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = VK_BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VK_EndSingleTimeCommands(commandBuffer);
}

//----------------------------------------------------------------------------
// Generate mipmaps (GPU-based)
//----------------------------------------------------------------------------
void VK_GenerateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(g_vkDevice.physicalDevice, format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        ri.Printf(PRINT_WARNING, "WARNING: Texture image format does not support linear blitting, skipping mipmap generation\n");
        return;
    }

    VkCommandBuffer commandBuffer = VK_BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit = {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VK_EndSingleTimeCommands(commandBuffer);
}

//----------------------------------------------------------------------------
// Create descriptor set for image
//----------------------------------------------------------------------------
void VK_CreateImageDescriptorSet(vkImage_t* vkImg) {
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_vkPipelines.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &g_vkPipelines.setLayouts[2];  // Set 2: Textures

    VK_CHECK(vkAllocateDescriptorSets(g_vkDevice.device, &allocInfo, &vkImg->descriptorSet));

    // Update descriptor set
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = vkImg->view;
    imageInfo.sampler = vkImg->sampler;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = vkImg->descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(g_vkDevice.device, 1, &descriptorWrite, 0, nullptr);
}

//----------------------------------------------------------------------------
// Create image from pixel data
//----------------------------------------------------------------------------
void VK_CreateImageFromPixels(const image_t* image, const byte* pic, qboolean isLightmap) {
    if (!image) {
        ri.Error(ERR_FATAL, "VK_CreateImageFromPixels: NULL image pointer\n");
        return;
    }

    if (!pic) {
        ri.Error(ERR_FATAL, "VK_CreateImageFromPixels: NULL pixel data\n");
        return;
    }

    if (image->width <= 0 || image->height <= 0 || image->width > 8192 || image->height > 8192) {
        ri.Error(ERR_FATAL, "VK_CreateImageFromPixels: Invalid image dimensions %dx%d\n", image->width, image->height);
        return;
    }

    vkImage_t* vkImg = VK_GetImage(image);
    if (!vkImg) {
        ri.Error(ERR_FATAL, "VK_CreateImageFromPixels: Failed to get vkImage_t for index %d\n", image->index);
        return;
    }

    // Determine format
    VkFormat format;
    int bytesPerPixel;

    switch (image->format) {
        case IMAGEFORMAT_RGBA8:
        case IMAGEFORMAT_RGBA:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            bytesPerPixel = 4;
            break;
        case IMAGEFORMAT_RGB8:
        case IMAGEFORMAT_RGB:
            format = VK_FORMAT_R8G8B8A8_UNORM;  // Convert RGB to RGBA
            bytesPerPixel = 3;
            break;
        default:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            bytesPerPixel = 4;
            break;
    }

    // Calculate mip levels
    uint32_t mipLevels = 1;
    if (image->mipmap && !isLightmap) {
        mipLevels = static_cast<uint32_t>(floor(log2(max(image->width, image->height)))) + 1;
    }

    // Store info
    vkImg->width = image->width;
    vkImg->height = image->height;
    vkImg->format = format;
    vkImg->dynamic = qfalse;

    // Create staging buffer
    VkDeviceSize imageSize = image->width * image->height * 4;  // Always RGBA for upload
    void* stagingData;
    VmaAllocation stagingAllocation;
    VkBuffer stagingBuffer = VK_CreateStagingBuffer(imageSize, &stagingAllocation, &stagingData);

    // Copy pixel data (convert RGB to RGBA if needed)
    if (bytesPerPixel == 3) {
        byte* dst = (byte*)stagingData;
        for (int i = 0; i < image->width * image->height; i++) {
            dst[i * 4 + 0] = pic[i * 3 + 0];
            dst[i * 4 + 1] = pic[i * 3 + 1];
            dst[i * 4 + 2] = pic[i * 3 + 2];
            dst[i * 4 + 3] = 255;
        }
    } else {
        memcpy(stagingData, pic, imageSize);
    }

    // Apply light scaling for consistent brightness with D3D11/OpenGL backends
    // only_gamma is true when there are no mipmaps (mipLevels == 1)
    R_LightScaleTexture((unsigned int*)stagingData, image->width, image->height, (qboolean)(mipLevels == 1));

    // Create image
    vkImg->image = VK_CreateImage2D(
        image->width, image->height, mipLevels,
        VK_SAMPLE_COUNT_1_BIT, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        &vkImg->allocation);

    // Transition to transfer dst
    VK_TransitionImageLayout(vkImg->image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

    // Copy from staging buffer
    VK_CopyBufferToImage(stagingBuffer, vkImg->image, image->width, image->height);

    // Generate mipmaps (also transitions to shader read)
    if (mipLevels > 1) {
        VK_GenerateMipmaps(vkImg->image, format, image->width, image->height, mipLevels);
    } else {
        VK_TransitionImageLayout(vkImg->image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }

    // Clean up staging buffer
    VK_DestroyStagingBuffer(stagingBuffer, stagingAllocation);

    // Create image view
    vkImg->view = VK_CreateImageView(vkImg->image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

    // Create sampler
    vkImg->sampler = VK_CreateSampler(image->mipmap, image->wrapClampMode);

    // Create descriptor set
    VK_CreateImageDescriptorSet(vkImg);
}

//----------------------------------------------------------------------------
// Delete image
//----------------------------------------------------------------------------
void VK_DeleteImageInternal(const image_t* image) {
    vkImage_t* vkImg = VK_GetImage(image);
    if (!vkImg || vkImg->image == VK_NULL_HANDLE) {
        return;
    }

    // Note: Caller is responsible for ensuring GPU has finished using this image
    // (via VK_EndFrame() + VK_FlushGPU() or similar synchronization)

    // Destroy staging buffer if exists (only dynamic images have persistent staging buffers)
    if (vkImg->stagingBuffer != VK_NULL_HANDLE) {
        // Only unmap if it was persistently mapped (dynamic images only)
        if (vkImg->dynamic && vkImg->stagingMappedData != nullptr) {
            vmaUnmapMemory(g_vkDevice.allocator, vkImg->stagingAllocation);
        }
        vmaDestroyBuffer(g_vkDevice.allocator, vkImg->stagingBuffer, vkImg->stagingAllocation);
    }

    // Free descriptor set
    if (vkImg->descriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(g_vkDevice.device, g_vkPipelines.descriptorPool, 1, &vkImg->descriptorSet);
    }

    // Destroy sampler
    if (vkImg->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_vkDevice.device, vkImg->sampler, nullptr);
    }

    // Destroy image view
    if (vkImg->view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_vkDevice.device, vkImg->view, nullptr);
    }

    // Destroy image
    vmaDestroyImage(g_vkDevice.allocator, vkImg->image, vkImg->allocation);

    // Zero out structure
    memset(vkImg, 0, sizeof(vkImage_t));
}

//----------------------------------------------------------------------------
// Cleanup all images (called during shutdown)
//----------------------------------------------------------------------------
void VK_DestroyAllImages() {
    int destroyedCount = 0;
    for (int i = 0; i < VK_MAX_IMAGES; i++) {
        if (g_vkImages[i].image != VK_NULL_HANDLE) {
            // Destroy staging buffer if exists (only dynamic images have persistent staging buffers)
            if (g_vkImages[i].stagingBuffer != VK_NULL_HANDLE) {
                // Only unmap if it was persistently mapped (dynamic images only)
                if (g_vkImages[i].dynamic && g_vkImages[i].stagingMappedData != nullptr) {
                    vmaUnmapMemory(g_vkDevice.allocator, g_vkImages[i].stagingAllocation);
                    g_vkImages[i].stagingMappedData = nullptr;
                }
                vmaDestroyBuffer(g_vkDevice.allocator, g_vkImages[i].stagingBuffer, g_vkImages[i].stagingAllocation);
                g_vkImages[i].stagingBuffer = VK_NULL_HANDLE;
            }

            // Free descriptor set
            if (g_vkImages[i].descriptorSet != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(g_vkDevice.device, g_vkPipelines.descriptorPool, 1, &g_vkImages[i].descriptorSet);
            }

            // Destroy sampler
            if (g_vkImages[i].sampler != VK_NULL_HANDLE) {
                vkDestroySampler(g_vkDevice.device, g_vkImages[i].sampler, nullptr);
            }

            // Destroy image view
            if (g_vkImages[i].view != VK_NULL_HANDLE) {
                vkDestroyImageView(g_vkDevice.device, g_vkImages[i].view, nullptr);
            }

            // Destroy image
            vmaDestroyImage(g_vkDevice.allocator, g_vkImages[i].image, g_vkImages[i].allocation);

            // Zero out structure
            memset(&g_vkImages[i], 0, sizeof(vkImage_t));

            destroyedCount++;
        }
    }
}

//----------------------------------------------------------------------------
// Update dynamic image (cinematics)
//----------------------------------------------------------------------------
void VK_UpdateDynamicImage(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty) {
    vkImage_t* vkImg = VK_GetImage(image);
    if (!vkImg) {
        return;
    }

    // If dimensions changed or image doesn't exist yet, recreate it
    if (vkImg->image == VK_NULL_HANDLE || cols != vkImg->width || rows != vkImg->height) {

        // CRITICAL: End render pass, submit current work, and wait for GPU
        // We must do this to safely modify resources without invalidating command buffers
        qboolean wasInRenderPass = g_vkDraw.inRenderPass;
        if (wasInRenderPass) {
            vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];
            vkCmdEndRenderPass(frame->commandBuffer);
            g_vkDraw.inRenderPass = qfalse;

            // End and submit the command buffer
            vkEndCommandBuffer(frame->commandBuffer);
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &frame->commandBuffer;
            vkQueueSubmit(g_vkDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        }

        VK_FlushGPU();

        // Store the old descriptor set to reuse it
        VkDescriptorSet oldDescriptorSet = vkImg->descriptorSet;

        // Destroy old image resources (GPU is idle, safe to destroy)
        if (vkImg->stagingBuffer != VK_NULL_HANDLE) {
            if (vkImg->stagingMappedData != nullptr) {
                vmaUnmapMemory(g_vkDevice.allocator, vkImg->stagingAllocation);
            }
            vmaDestroyBuffer(g_vkDevice.allocator, vkImg->stagingBuffer, vkImg->stagingAllocation);
        }
        if (vkImg->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(g_vkDevice.device, vkImg->sampler, nullptr);
        }
        if (vkImg->view != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vkDevice.device, vkImg->view, nullptr);
        }
        if (vkImg->image != VK_NULL_HANDLE) {
            vmaDestroyImage(g_vkDevice.allocator, vkImg->image, vkImg->allocation);
        }

        // Clear the structure but preserve the descriptor set
        memset(vkImg, 0, sizeof(vkImage_t));
        vkImg->descriptorSet = oldDescriptorSet;

        // Create new image
        image_t tempImage = *image;
        tempImage.width = cols;
        tempImage.height = rows;
        tempImage.mipmap = qfalse;  // Cinematics don't use mipmaps

        VK_CreateImageFromPixels(&tempImage, pic, qfalse);
        vkImg->dynamic = qtrue;

        // If we had an old descriptor set, update it to point to the new image
        // Otherwise VK_CreateImageFromPixels already created one
        if (oldDescriptorSet != VK_NULL_HANDLE) {
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = vkImg->view;
            imageInfo.sampler = vkImg->sampler;

            VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = vkImg->descriptorSet;
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(g_vkDevice.device, 1, &descriptorWrite, 0, nullptr);
        }

        // Resume rendering if we paused it - restart the command buffer and render pass
        if (wasInRenderPass) {
            vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];

            // Reset and restart command buffer
            vkResetCommandBuffer(frame->commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(frame->commandBuffer, &beginInfo);

            // Restart render pass
            VkClearValue clearValues[3];
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[2].depthStencil = {1.0f, 0};
            qboolean msaaEnabled = (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? qtrue : qfalse;

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = g_vkDevice.renderPass;
            renderPassInfo.framebuffer = g_vkDevice.framebuffers[g_vkDraw.imageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = g_vkDevice.swapchainExtent;
            renderPassInfo.clearValueCount = msaaEnabled ? 3 : 2;
            renderPassInfo.pClearValues = clearValues;
            vkCmdBeginRenderPass(frame->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Restore viewport and scissor
            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = (float)g_vkDevice.swapchainExtent.height;
            viewport.width = (float)g_vkDevice.swapchainExtent.width;
            viewport.height = -(float)g_vkDevice.swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = g_vkDevice.swapchainExtent;
            vkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

            g_vkDraw.inRenderPass = qtrue;
        }

        return;
    }

    // Update existing image
    if (dirty) {
        // Create staging buffer on first update
        if (vkImg->stagingBuffer == VK_NULL_HANDLE) {
            VkDeviceSize size = cols * rows * 4;
            vkImg->stagingBuffer = VK_CreateStagingBuffer(size, &vkImg->stagingAllocation,
                                                          &vkImg->stagingMappedData);
            vkImg->dynamic = qtrue;
        }

        // Copy to staging buffer
        memcpy(vkImg->stagingMappedData, pic, cols * rows * 4);

        // CRITICAL: Pause rendering, submit work, and wait for GPU
        qboolean wasInRenderPass = g_vkDraw.inRenderPass;
        if (wasInRenderPass) {
            vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];
            vkCmdEndRenderPass(frame->commandBuffer);
            g_vkDraw.inRenderPass = qfalse;

            // End and submit
            vkEndCommandBuffer(frame->commandBuffer);
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &frame->commandBuffer;
            vkQueueSubmit(g_vkDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        }

        VK_FlushGPU();

        // Update using transfer pool (now safe - GPU is idle)
        VK_TransitionImageLayout(vkImg->image, vkImg->format,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
        VK_CopyBufferToImage(vkImg->stagingBuffer, vkImg->image, cols, rows);
        VK_TransitionImageLayout(vkImg->image, vkImg->format,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

        // Resume rendering if we paused it
        if (wasInRenderPass) {
            vkFrameData_t* frame = &g_vkDevice.frames[g_vkDraw.currentFrame];

            // Reset and restart command buffer
            vkResetCommandBuffer(frame->commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(frame->commandBuffer, &beginInfo);

            // Restart render pass
            VkClearValue clearValues[3];
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[2].depthStencil = {1.0f, 0};
            qboolean msaaEnabled = (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? qtrue : qfalse;

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = g_vkDevice.renderPass;
            renderPassInfo.framebuffer = g_vkDevice.framebuffers[g_vkDraw.imageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = g_vkDevice.swapchainExtent;
            renderPassInfo.clearValueCount = msaaEnabled ? 3 : 2;
            renderPassInfo.pClearValues = clearValues;
            vkCmdBeginRenderPass(frame->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Restore viewport and scissor
            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = (float)g_vkDevice.swapchainExtent.height;
            viewport.width = (float)g_vkDevice.swapchainExtent.width;
            viewport.height = -(float)g_vkDevice.swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(frame->commandBuffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = g_vkDevice.swapchainExtent;
            vkCmdSetScissor(frame->commandBuffer, 0, 1, &scissor);

            g_vkDraw.inRenderPass = qtrue;
        }
    }
}

//----------------------------------------------------------------------------
// Get image format
//----------------------------------------------------------------------------
imageFormat_t VK_GetImageFormatInternal(const image_t* image) {
    vkImage_t* vkImg = VK_GetImage(image);
    if (!vkImg || vkImg->image == VK_NULL_HANDLE) {
        return IMAGEFORMAT_RGBA8;
    }

    // Map Vulkan format back to imageFormat_t
    switch (vkImg->format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return IMAGEFORMAT_RGBA8;
        default:
            return IMAGEFORMAT_RGBA8;
    }
}
