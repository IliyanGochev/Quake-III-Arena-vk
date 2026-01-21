#pragma once

#include "vk_common.h"

//----------------------------------------------------------------------------
// Image structure (analogous to D3D11's d3dImage_t)
//----------------------------------------------------------------------------
struct vkImage_t {
    VkImage image;
    VkImageView view;
    VkSampler sampler;
    VmaAllocation allocation;
    VkFormat format;
    int width;
    int height;
    int frameUsed;
    qboolean dynamic;  // For cinematics

    // Descriptor set for this image (pre-allocated)
    VkDescriptorSet descriptorSet;

    // Staging buffer for dynamic images
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    void* stagingMappedData;
};

//----------------------------------------------------------------------------
// Image storage
//----------------------------------------------------------------------------
#define VK_MAX_IMAGES 2048

//----------------------------------------------------------------------------
// Global image array
//----------------------------------------------------------------------------
extern vkImage_t g_vkImages[VK_MAX_IMAGES];

//----------------------------------------------------------------------------
// Image management functions
//----------------------------------------------------------------------------

// Initialize image system
void VK_InitImages();

// Shutdown image system
void VK_ShutdownImages();

// Create image from pixel data
void VK_CreateImageFromPixels(const image_t* image, const byte* pic, qboolean isLightmap);

// Delete image
void VK_DeleteImageInternal(const image_t* image);

// Destroy all images (cleanup during shutdown)
void VK_DestroyAllImages();

// Update dynamic image (cinematics)
void VK_UpdateDynamicImage(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty);

// Get image format
imageFormat_t VK_GetImageFormatInternal(const image_t* image);

// Get Vulkan image by engine image index
vkImage_t* VK_GetImage(const image_t* image);

//----------------------------------------------------------------------------
// Image creation helpers
//----------------------------------------------------------------------------

// Create VkImage
VkImage VK_CreateImage2D(uint32_t width, uint32_t height, uint32_t mipLevels,
                         VkSampleCountFlagBits numSamples, VkFormat format,
                         VkImageTiling tiling, VkImageUsageFlags usage,
                         VmaAllocation* allocation);

// Create image view
VkImageView VK_CreateImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags, uint32_t mipLevels);

// Create sampler
VkSampler VK_CreateSampler(qboolean mipmap, wrapClampMode_t wrapMode);

// Transition image layout
void VK_TransitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels);

// Copy buffer to image
void VK_CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

// Generate mipmaps
void VK_GenerateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

// Create descriptor set for image
void VK_CreateImageDescriptorSet(vkImage_t* vkImg);

//----------------------------------------------------------------------------
// Upload helpers
//----------------------------------------------------------------------------

// Create staging buffer
VkBuffer VK_CreateStagingBuffer(VkDeviceSize size, VmaAllocation* allocation, void** mappedData);

// Destroy staging buffer
void VK_DestroyStagingBuffer(VkBuffer buffer, VmaAllocation allocation);

// Begin single-time commands
VkCommandBuffer VK_BeginSingleTimeCommands();

// End single-time commands
void VK_EndSingleTimeCommands(VkCommandBuffer commandBuffer);

