// Vulkan rendering backend - image/texture management implementation

#include "vk_image.h"
#include "vk_state.h"
#include "vk_buffers.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================

#define MAX_VK_IMAGES 4096

static vkImage_t s_images[MAX_VK_IMAGES];
static int s_totalImageMemory = 0;

// Staging buffer for uploads
static VkBuffer s_stagingBuffer = VK_NULL_HANDLE;
static VmaAllocation s_stagingAllocation = VK_NULL_HANDLE;
static void* s_stagingMapped = NULL;
static size_t s_stagingSize = 4 * 1024 * 1024; // 4MB

//=============================================================================
// Initialization
//=============================================================================

void VkImage_Init(void)
{
    Com_Memset(s_images, 0, sizeof(s_images));
    s_totalImageMemory = 0;

    // Create staging buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = s_stagingSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocResult;
    vmaCreateBuffer(g_vmaAllocator, &bufferInfo, &allocInfo,
        &s_stagingBuffer, &s_stagingAllocation, &allocResult);
    s_stagingMapped = allocResult.pMappedData;
}

void VkImage_Shutdown(void)
{
    // Delete all images
    for (int i = 0; i < MAX_VK_IMAGES; i++) {
        if (s_images[i].inUse) {
            if (s_images[i].view) {
                qvkDestroyImageView(vk.device, s_images[i].view, NULL);
            }
            if (s_images[i].image) {
                vmaDestroyImage(g_vmaAllocator, s_images[i].image, s_images[i].allocation);
            }
            if (s_images[i].sampler) {
                qvkDestroySampler(vk.device, s_images[i].sampler, NULL);
            }
        }
    }

    // Destroy staging buffer
    if (s_stagingBuffer) {
        vmaDestroyBuffer(g_vmaAllocator, s_stagingBuffer, s_stagingAllocation);
        s_stagingBuffer = VK_NULL_HANDLE;
        s_stagingAllocation = VK_NULL_HANDLE;
        s_stagingMapped = NULL;
    }
}

//=============================================================================
// Format conversion
//=============================================================================

static VkFormat GetVkFormat(imageFormat_t format)
{
    switch (format) {
        case IMAGEFORMAT_RGBA8:
        case IMAGEFORMAT_RGBA:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case IMAGEFORMAT_RGB8:
        case IMAGEFORMAT_RGB:
            return VK_FORMAT_R8G8B8A8_UNORM; // Expand to 4 channels
        case IMAGEFORMAT_RGBA4:
            return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case IMAGEFORMAT_RGB5:
            return VK_FORMAT_R5G6B5_UNORM_PACK16;
        case IMAGEFORMAT_I:
            return VK_FORMAT_R8_UNORM;
        case IMAGEFORMAT_IA:
            return VK_FORMAT_R8G8_UNORM;
        default:
            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static imageFormat_t GetImageFormat(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return IMAGEFORMAT_RGBA8;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            return IMAGEFORMAT_RGBA4;
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
            return IMAGEFORMAT_RGB5;
        case VK_FORMAT_R8_UNORM:
            return IMAGEFORMAT_I;
        case VK_FORMAT_R8G8_UNORM:
            return IMAGEFORMAT_IA;
        default:
            return IMAGEFORMAT_RGBA8;
    }
}

//=============================================================================
// Image creation
//=============================================================================

static uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = (width > 1) ? width / 2 : 1;
        height = (height > 1) ? height / 2 : 1;
        levels++;
    }
    return levels;
}

void VkImage_Create(const image_t* image, const byte* pic, qboolean isLightmap)
{
    if (!image || image->index >= MAX_VK_IMAGES) {
        return;
    }

    vkImage_t* vkImg = &s_images[image->index];

    // Delete existing if any
    if (vkImg->inUse) {
        VkImage_Delete(image);
    }

    // Calculate scaled dimensions (power of 2)
    int scaledWidth, scaledHeight;
    for (scaledWidth = 1; scaledWidth < image->width; scaledWidth <<= 1)
        ;
    for (scaledHeight = 1; scaledHeight < image->height; scaledHeight <<= 1)
        ;

    // Apply r_roundImagesDown
    cvar_t* r_roundImagesDown = ri.Cvar_Get("r_roundImagesDown", "1", 0);
    if (r_roundImagesDown->integer && scaledWidth > image->width)
        scaledWidth >>= 1;
    if (r_roundImagesDown->integer && scaledHeight > image->height)
        scaledHeight >>= 1;

    // Apply picmip
    if (image->allowPicmip) {
        cvar_t* r_picmip = ri.Cvar_Get("r_picmip", "1", 0);
        scaledWidth >>= r_picmip->integer;
        scaledHeight >>= r_picmip->integer;
    }

    // Clamp to minimum size
    if (scaledWidth < 1) scaledWidth = 1;
    if (scaledHeight < 1) scaledHeight = 1;

    // Clamp to max texture size
    while (scaledWidth > (int)vk.deviceProperties.limits.maxImageDimension2D ||
           scaledHeight > (int)vk.deviceProperties.limits.maxImageDimension2D) {
        scaledWidth >>= 1;
        scaledHeight >>= 1;
    }

    vkImg->width = scaledWidth;
    vkImg->height = scaledHeight;
    vkImg->format = GetVkFormat(image->format);
    vkImg->mipLevels = image->mipmap ? CalculateMipLevels(scaledWidth, scaledHeight) : 1;
    vkImg->dynamic = qfalse;

    // Create image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = scaledWidth;
    imageInfo.extent.height = scaledHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = vkImg->mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vkImg->format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(g_vmaAllocator, &imageInfo, &allocInfo,
        &vkImg->image, &vkImg->allocation, NULL);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vmaCreateImage failed for %s: %s\n", image->imgName, Vk_ResultString(result));
        return;
    }

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vkImg->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkImg->format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = vkImg->mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = qvkCreateImageView(vk.device, &viewInfo, NULL, &vkImg->view);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateImageView failed: %s\n", Vk_ResultString(result));
        vmaDestroyImage(g_vmaAllocator, vkImg->image, vkImg->allocation);
        return;
    }

    // Create sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = image->mipmap ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = image->mipmap ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = image->mipmap ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (image->wrapClampMode == WRAPMODE_CLAMP) {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    } else {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    samplerInfo.anisotropyEnable = vk.deviceFeatures.samplerAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = vk.deviceFeatures.samplerAnisotropy ?
        vk.deviceProperties.limits.maxSamplerAnisotropy : 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)vkImg->mipLevels;

    result = qvkCreateSampler(vk.device, &samplerInfo, NULL, &vkImg->sampler);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateSampler failed: %s\n", Vk_ResultString(result));
    }

    // Upload image data
    if (pic) {
        // Use the stored dimensions for consistency
        int uploadWidth = vkImg->width;
        int uploadHeight = vkImg->height;

        const byte* uploadData = pic;
        byte* resampledBuffer = NULL;

        // Resample if dimensions changed from source
        if (uploadWidth != image->width || uploadHeight != image->height) {
            resampledBuffer = (byte*)ri.Hunk_AllocateTempMemory(uploadWidth * uploadHeight * 4);
            R_ResampleTexture((unsigned*)pic, image->width, image->height,
                              (unsigned*)resampledBuffer, uploadWidth, uploadHeight);
            uploadData = resampledBuffer;
        }

        // Light scale the texture (match OpenGL/D3D11 behavior)
        byte* lightScaled = (byte*)ri.Hunk_AllocateTempMemory(uploadWidth * uploadHeight * 4);
        memcpy(lightScaled, uploadData, uploadWidth * uploadHeight * 4);
        R_LightScaleTexture((unsigned*)lightScaled, uploadWidth, uploadHeight, (qboolean)(vkImg->mipLevels == 1));

        size_t baseImageSize = uploadWidth * uploadHeight * 4;
        if (baseImageSize <= s_stagingSize) {
            // Transition entire image to transfer dst
            Vk_TransitionImageLayout(vkImg->image, vkImg->format,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vkImg->mipLevels);

            // Upload mip level 0 (base level)
            memcpy(s_stagingMapped, lightScaled, baseImageSize);
            Vk_CopyBufferToImage(s_stagingBuffer, vkImg->image, uploadWidth, uploadHeight);

            // Generate and upload remaining mip levels (like D3D11)
            if (vkImg->mipLevels > 1) {
                int mipWidth = uploadWidth;
                int mipHeight = uploadHeight;

                for (uint32_t mip = 1; mip < vkImg->mipLevels; mip++) {
                    // Downsample the lightScaled buffer in-place (matches D3D11's R_MipMap usage)
                    R_MipMap(lightScaled, mipWidth, mipHeight);
                    mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
                    mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;

                    // Upload this mip level
                    size_t mipSize = mipWidth * mipHeight * 4;
                    memcpy(s_stagingMapped, lightScaled, mipSize);
                    Vk_CopyBufferToImageMip(s_stagingBuffer, vkImg->image, mipWidth, mipHeight, mip);
                }
            }

            // Transition to shader read
            Vk_TransitionImageLayout(vkImg->image, vkImg->format,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkImg->mipLevels);
        }

        ri.Hunk_FreeTempMemory(lightScaled);
        if (resampledBuffer) {
            ri.Hunk_FreeTempMemory(resampledBuffer);
        }
    }

    // Create per-frame descriptor sets for this image
    VkDescriptorSetLayout layouts[VK_MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSetLayout layout = VkState_GetTextureSetLayout();
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = layout;
    }

    VkDescriptorSetAllocateInfo descAllocInfo = {};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = vk.descriptorPool;
    descAllocInfo.descriptorSetCount = VK_MAX_FRAMES_IN_FLIGHT;
    descAllocInfo.pSetLayouts = layouts;

    result = qvkAllocateDescriptorSets(vk.device, &descAllocInfo, vkImg->descriptorSets);
    if (result == VK_SUCCESS) {
        // Update all descriptor sets - write UBOs and texture
        // Use the ring buffer for dynamic uniform data (per-draw offsets at bind time)
        VkBuffer uniformBuffer = VkBuffers_GetUniformBuffer();

        // Skip UBO binding if buffer not yet created
        if (!uniformBuffer) {
            return;
        }

        // For dynamic UBOs, offset=0 and range=struct size (actual offset at bind time)
        VkDescriptorBufferInfo vsBufferInfo = {};
        vsBufferInfo.buffer = uniformBuffer;
        vsBufferInfo.offset = 0;
        vsBufferInfo.range = VkState_GetVSUniformSize();

        VkDescriptorBufferInfo psBufferInfo = {};
        psBufferInfo.buffer = uniformBuffer;
        psBufferInfo.offset = 0;
        psBufferInfo.range = VkState_GetPSUniformSize();

        // Separate image info (no sampler) for SAMPLED_IMAGE type
        VkDescriptorImageInfo imageOnlyInfo = {};
        imageOnlyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageOnlyInfo.imageView = vkImg->view;
        imageOnlyInfo.sampler = VK_NULL_HANDLE;  // Not used for SAMPLED_IMAGE

        // Sampler-only info for SAMPLER type
        VkDescriptorImageInfo samplerOnlyInfo = {};
        samplerOnlyInfo.sampler = vkImg->sampler;

        // Update each frame's descriptor set
        for (int frame = 0; frame < VK_MAX_FRAMES_IN_FLIGHT; frame++) {
            VkWriteDescriptorSet descriptorWrites[5] = {};

            // Binding 0: VS Uniform Buffer (dynamic - offset at bind time)
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = vkImg->descriptorSets[frame];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &vsBufferInfo;

            // Binding 1: PS Uniform Buffer (dynamic - offset at bind time)
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = vkImg->descriptorSets[frame];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &psBufferInfo;

            // Binding 2: Diffuse texture (sampled image, separate from sampler)
            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = vkImg->descriptorSets[frame];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &imageOnlyInfo;

            // Binding 3: Lightmap texture (for multi-texture; use same image as placeholder)
            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = vkImg->descriptorSets[frame];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &imageOnlyInfo;  // Placeholder; multi-texture will update

            // Binding 4: Sampler
            descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[4].dstSet = vkImg->descriptorSets[frame];
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].dstArrayElement = 0;
            descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pImageInfo = &samplerOnlyInfo;

            qvkUpdateDescriptorSets(vk.device, 5, descriptorWrites, 0, NULL);
        }
    }

    vkImg->memorySize = vkImg->width * vkImg->height * 4;
    s_totalImageMemory += (int)vkImg->memorySize;
    vkImg->inUse = qtrue;
}

void VkImage_Delete(const image_t* image)
{
    if (!image || image->index >= MAX_VK_IMAGES) {
        return;
    }

    vkImage_t* vkImg = &s_images[image->index];

    if (!vkImg->inUse) {
        return;
    }

    // Wait for GPU to finish using this image
    qvkDeviceWaitIdle(vk.device);

    // Free all per-frame descriptor sets
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkImg->descriptorSets[i]) {
            qvkFreeDescriptorSets(vk.device, vk.descriptorPool, 1, &vkImg->descriptorSets[i]);
        }
    }

    if (vkImg->sampler) {
        qvkDestroySampler(vk.device, vkImg->sampler, NULL);
    }

    if (vkImg->view) {
        qvkDestroyImageView(vk.device, vkImg->view, NULL);
    }

    if (vkImg->image) {
        vmaDestroyImage(g_vmaAllocator, vkImg->image, vkImg->allocation);
    }

    s_totalImageMemory -= (int)vkImg->memorySize;

    Com_Memset(vkImg, 0, sizeof(*vkImg));
}

void VkImage_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty)
{
    if (!image || image->index >= MAX_VK_IMAGES || !dirty) {
        return;
    }

    vkImage_t* vkImg = &s_images[image->index];

    // If dimensions changed or image doesn't exist, recreate it
    if (!vkImg->inUse || (int)vkImg->width != cols || (int)vkImg->height != rows) {
        // Delete old image if exists
        if (vkImg->inUse) {
            VkImage_Delete(image);
        }

        // Create a temporary image_t with the cinematic dimensions
        // We need to create a new Vulkan image with the correct size
        image_t tempImage = *image;
        tempImage.width = cols;
        tempImage.height = rows;
        tempImage.mipmap = qfalse;
        tempImage.allowPicmip = qfalse;  // Don't apply picmip to cinematics

        VkImage_Create(&tempImage, pic, qfalse);
        return;
    }

    // For cinematics with matching dimensions, just re-upload the texture data
    size_t imageSize = cols * rows * 4;
    if (imageSize <= s_stagingSize && pic) {
        memcpy(s_stagingMapped, pic, imageSize);

        // Transition to transfer dst
        Vk_TransitionImageLayout(vkImg->image, vkImg->format,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

        // Copy buffer to image
        Vk_CopyBufferToImage(s_stagingBuffer, vkImg->image, cols, rows);

        // Transition back to shader read
        Vk_TransitionImageLayout(vkImg->image, vkImg->format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }
}

imageFormat_t VkImage_GetFormat(const image_t* image)
{
    if (!image || image->index >= MAX_VK_IMAGES) {
        return IMAGEFORMAT_RGBA8;
    }

    vkImage_t* vkImg = &s_images[image->index];
    return GetImageFormat(vkImg->format);
}

int VkImage_GetMemoryUsage(void)
{
    return s_totalImageMemory;
}

vkImage_t* VkImage_GetData(const image_t* image)
{
    if (!image || image->index >= MAX_VK_IMAGES) {
        return NULL;
    }

    vkImage_t* vkImg = &s_images[image->index];
    return vkImg->inUse ? vkImg : NULL;
}

VkDescriptorSet VkImage_GetDescriptorSet(const image_t* image)
{
    vkImage_t* vkImg = VkImage_GetData(image);
    if (!vkImg) {
        return VK_NULL_HANDLE;
    }
    // Return the descriptor set for the current frame in flight
    return vkImg->descriptorSets[vk.currentFrame];
}

void VkImage_BindLightmap(const image_t* diffuse, const image_t* lightmap)
{
    if (!diffuse || !lightmap) {
        return;
    }

    vkImage_t* vkDiffuse = VkImage_GetData(diffuse);
    vkImage_t* vkLightmap = VkImage_GetData(lightmap);

    if (!vkDiffuse || !vkLightmap) {
        return;
    }

    // Get current frame's descriptor set
    VkDescriptorSet currentSet = vkDiffuse->descriptorSets[vk.currentFrame];
    if (!currentSet) {
        return;
    }

    // Update binding 3 (lightmap texture) of the current frame's descriptor set only
    // This is safe because this frame's descriptor set is not in use by any pending command buffer
    VkDescriptorImageInfo lightmapInfo = {};
    lightmapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lightmapInfo.imageView = vkLightmap->view;
    lightmapInfo.sampler = VK_NULL_HANDLE;  // Not used for SAMPLED_IMAGE

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = currentSet;
    descriptorWrite.dstBinding = 3;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &lightmapInfo;

    qvkUpdateDescriptorSets(vk.device, 1, &descriptorWrite, 0, NULL);
}

VkDescriptorSet VkImage_AllocMultitextureSet(const image_t* diffuse, const image_t* lightmap)
{
    if (!diffuse || !lightmap) {
        return VK_NULL_HANDLE;
    }

    vkImage_t* vkDiffuse = VkImage_GetData(diffuse);
    vkImage_t* vkLightmap = VkImage_GetData(lightmap);

    if (!vkDiffuse || !vkLightmap) {
        return VK_NULL_HANDLE;
    }

    vkFrame_t* frame = &vk.frames[vk.currentFrame];
    if (!frame->dynamicDescriptorPool) {
        return VK_NULL_HANDLE;
    }

    // Allocate a fresh descriptor set from the frame's dynamic pool
    VkDescriptorSetLayout layout = VkState_GetTextureSetLayout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = frame->dynamicDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descSet;
    VkResult result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &descSet);
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    // Get UBO info (use ring buffer for dynamic UBOs)
    VkBuffer uniformBuffer = VkBuffers_GetUniformBuffer();
    if (!uniformBuffer) {
        return VK_NULL_HANDLE;
    }

    // For dynamic UBOs, offset=0 and range=struct size (actual offset at bind time)
    VkDescriptorBufferInfo vsBufferInfo = {};
    vsBufferInfo.buffer = uniformBuffer;
    vsBufferInfo.offset = 0;
    vsBufferInfo.range = VkState_GetVSUniformSize();

    VkDescriptorBufferInfo psBufferInfo = {};
    psBufferInfo.buffer = uniformBuffer;
    psBufferInfo.offset = 0;
    psBufferInfo.range = VkState_GetPSUniformSize();

    // Image infos
    VkDescriptorImageInfo diffuseInfo = {};
    diffuseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    diffuseInfo.imageView = vkDiffuse->view;
    diffuseInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo lightmapInfo = {};
    lightmapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lightmapInfo.imageView = vkLightmap->view;
    lightmapInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.sampler = vkDiffuse->sampler;

    VkWriteDescriptorSet writes[5] = {};

    // Binding 0: VS UBO (dynamic - offset at bind time)
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &vsBufferInfo;

    // Binding 1: PS UBO (dynamic - offset at bind time)
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &psBufferInfo;

    // Binding 2: Diffuse texture
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &diffuseInfo;

    // Binding 3: Lightmap texture
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = descSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &lightmapInfo;

    // Binding 4: Sampler
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = descSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &samplerInfo;

    qvkUpdateDescriptorSets(vk.device, 5, writes, 0, NULL);

    return descSet;
}
