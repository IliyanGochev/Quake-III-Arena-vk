// @pjb: Vulkan rendering backend - image/texture management
#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "vk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Per-image Vulkan data (C++ only due to VmaAllocation)
//=============================================================================

#ifdef __cplusplus
typedef struct vkImage_s {
    VkImage             image;
    VkImageView         view;
    VmaAllocation       allocation;
    VkSampler           sampler;
    VkDescriptorSet     descriptorSet;
    VkFormat            format;
    uint32_t            width;
    uint32_t            height;
    uint32_t            mipLevels;
    qboolean            dynamic;        // For cinematics
    qboolean            inUse;
    size_t              memorySize;
} vkImage_t;
#endif

//=============================================================================
// Functions
//=============================================================================

void VkImage_Init(void);
void VkImage_Shutdown(void);

void VkImage_Create(const image_t* image, const byte* pic, qboolean isLightmap);
void VkImage_Delete(const image_t* image);
void VkImage_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty);

imageFormat_t VkImage_GetFormat(const image_t* image);
int VkImage_GetMemoryUsage(void);

// Get Vulkan-specific image data
vkImage_t* VkImage_GetData(const image_t* image);

// Bind image to descriptor set
VkDescriptorSet VkImage_GetDescriptorSet(const image_t* image);

#ifdef __cplusplus
}
#endif

#endif // VK_IMAGE_H
