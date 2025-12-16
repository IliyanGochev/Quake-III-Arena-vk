// @pjb: Vulkan rendering backend - common infrastructure
#ifndef VK_COMMON_H
#define VK_COMMON_H

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// VMA configuration - must be set before including vk_mem_alloc.h
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1000000  // Vulkan 1.0

// Include VMA - this is C++ only
#ifdef __cplusplus
#include "vk_mem_alloc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "../game/q_shared.h"
#include "../renderer/tr_local.h"
#include "../renderer/tr_layer.h"

//=============================================================================
// Constants
//=============================================================================

#define VK_MAX_FRAMES_IN_FLIGHT     2
#define VK_MAX_SWAPCHAIN_IMAGES     4
#define VK_MAX_DESCRIPTOR_SETS      4096
#define VK_VERTEX_BUFFER_SIZE       (4 * 1024 * 1024)   // 4MB
#define VK_INDEX_BUFFER_SIZE        (1 * 1024 * 1024)   // 1MB
#define VK_UNIFORM_BUFFER_SIZE      (256 * 1024)        // 256KB

//=============================================================================
// Vulkan function pointers (loaded dynamically)
//=============================================================================

// Instance-level functions
extern PFN_vkGetInstanceProcAddr                    qvkGetInstanceProcAddr;
extern PFN_vkCreateInstance                         qvkCreateInstance;
extern PFN_vkDestroyInstance                        qvkDestroyInstance;
extern PFN_vkEnumeratePhysicalDevices               qvkEnumeratePhysicalDevices;
extern PFN_vkGetPhysicalDeviceProperties            qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceFeatures              qvkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties      qvkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceFormatProperties      qvkGetPhysicalDeviceFormatProperties;
extern PFN_vkCreateDevice                           qvkCreateDevice;
extern PFN_vkDestroyDevice                          qvkDestroyDevice;
extern PFN_vkGetDeviceProcAddr                      qvkGetDeviceProcAddr;
extern PFN_vkGetDeviceQueue                         qvkGetDeviceQueue;

// Surface/swapchain functions
extern PFN_vkDestroySurfaceKHR                      qvkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR     qvkGetPhysicalDeviceSurfaceSupportKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR     qvkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR qvkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkCreateSwapchainKHR                     qvkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR                    qvkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR                  qvkGetSwapchainImagesKHR;
extern PFN_vkAcquireNextImageKHR                    qvkAcquireNextImageKHR;
extern PFN_vkQueuePresentKHR                        qvkQueuePresentKHR;

// Win32 surface
extern PFN_vkCreateWin32SurfaceKHR                  qvkCreateWin32SurfaceKHR;

// Device-level functions
extern PFN_vkQueueSubmit                            qvkQueueSubmit;
extern PFN_vkQueueWaitIdle                          qvkQueueWaitIdle;
extern PFN_vkDeviceWaitIdle                         qvkDeviceWaitIdle;

// Memory
extern PFN_vkAllocateMemory                         qvkAllocateMemory;
extern PFN_vkFreeMemory                             qvkFreeMemory;
extern PFN_vkMapMemory                              qvkMapMemory;
extern PFN_vkUnmapMemory                            qvkUnmapMemory;
extern PFN_vkFlushMappedMemoryRanges                qvkFlushMappedMemoryRanges;
extern PFN_vkInvalidateMappedMemoryRanges           qvkInvalidateMappedMemoryRanges;
extern PFN_vkBindBufferMemory                       qvkBindBufferMemory;
extern PFN_vkBindImageMemory                        qvkBindImageMemory;
extern PFN_vkGetBufferMemoryRequirements            qvkGetBufferMemoryRequirements;
extern PFN_vkGetImageMemoryRequirements             qvkGetImageMemoryRequirements;

// Buffers
extern PFN_vkCreateBuffer                           qvkCreateBuffer;
extern PFN_vkDestroyBuffer                          qvkDestroyBuffer;

// Images
extern PFN_vkCreateImage                            qvkCreateImage;
extern PFN_vkDestroyImage                           qvkDestroyImage;
extern PFN_vkCreateImageView                        qvkCreateImageView;
extern PFN_vkDestroyImageView                       qvkDestroyImageView;
extern PFN_vkCreateSampler                          qvkCreateSampler;
extern PFN_vkDestroySampler                         qvkDestroySampler;

// Render passes and framebuffers
extern PFN_vkCreateRenderPass                       qvkCreateRenderPass;
extern PFN_vkDestroyRenderPass                      qvkDestroyRenderPass;
extern PFN_vkCreateFramebuffer                      qvkCreateFramebuffer;
extern PFN_vkDestroyFramebuffer                     qvkDestroyFramebuffer;

// Shaders and pipelines
extern PFN_vkCreateShaderModule                     qvkCreateShaderModule;
extern PFN_vkDestroyShaderModule                    qvkDestroyShaderModule;
extern PFN_vkCreatePipelineLayout                   qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout                  qvkDestroyPipelineLayout;
extern PFN_vkCreateGraphicsPipelines                qvkCreateGraphicsPipelines;
extern PFN_vkDestroyPipeline                        qvkDestroyPipeline;
extern PFN_vkCreatePipelineCache                    qvkCreatePipelineCache;
extern PFN_vkDestroyPipelineCache                   qvkDestroyPipelineCache;
extern PFN_vkGetPipelineCacheData                   qvkGetPipelineCacheData;

// Descriptors
extern PFN_vkCreateDescriptorSetLayout              qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout             qvkDestroyDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool                   qvkCreateDescriptorPool;
extern PFN_vkDestroyDescriptorPool                  qvkDestroyDescriptorPool;
extern PFN_vkAllocateDescriptorSets                 qvkAllocateDescriptorSets;
extern PFN_vkFreeDescriptorSets                     qvkFreeDescriptorSets;
extern PFN_vkUpdateDescriptorSets                   qvkUpdateDescriptorSets;
extern PFN_vkResetDescriptorPool                    qvkResetDescriptorPool;

// Command pools and buffers
extern PFN_vkCreateCommandPool                      qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool                     qvkDestroyCommandPool;
extern PFN_vkResetCommandPool                       qvkResetCommandPool;
extern PFN_vkAllocateCommandBuffers                 qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers                     qvkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer                     qvkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer                       qvkEndCommandBuffer;
extern PFN_vkResetCommandBuffer                     qvkResetCommandBuffer;

// Command buffer commands
extern PFN_vkCmdBeginRenderPass                     qvkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass                       qvkCmdEndRenderPass;
extern PFN_vkCmdBindPipeline                        qvkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets                  qvkCmdBindDescriptorSets;
extern PFN_vkCmdBindVertexBuffers                   qvkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer                     qvkCmdBindIndexBuffer;
extern PFN_vkCmdSetViewport                         qvkCmdSetViewport;
extern PFN_vkCmdSetScissor                          qvkCmdSetScissor;
extern PFN_vkCmdSetDepthBias                        qvkCmdSetDepthBias;
extern PFN_vkCmdSetBlendConstants                   qvkCmdSetBlendConstants;
extern PFN_vkCmdDraw                                qvkCmdDraw;
extern PFN_vkCmdDrawIndexed                         qvkCmdDrawIndexed;
extern PFN_vkCmdCopyBuffer                          qvkCmdCopyBuffer;
extern PFN_vkCmdCopyBufferToImage                   qvkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage                           qvkCmdCopyImage;
extern PFN_vkCmdBlitImage                           qvkCmdBlitImage;
extern PFN_vkCmdClearColorImage                     qvkCmdClearColorImage;
extern PFN_vkCmdClearDepthStencilImage              qvkCmdClearDepthStencilImage;
extern PFN_vkCmdClearAttachments                    qvkCmdClearAttachments;
extern PFN_vkCmdPipelineBarrier                     qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants                       qvkCmdPushConstants;

// Synchronization
extern PFN_vkCreateSemaphore                        qvkCreateSemaphore;
extern PFN_vkDestroySemaphore                       qvkDestroySemaphore;
extern PFN_vkCreateFence                            qvkCreateFence;
extern PFN_vkDestroyFence                           qvkDestroyFence;
extern PFN_vkWaitForFences                          qvkWaitForFences;
extern PFN_vkResetFences                            qvkResetFences;
extern PFN_vkGetFenceStatus                         qvkGetFenceStatus;

// Debug (optional)
extern PFN_vkCreateDebugUtilsMessengerEXT           qvkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT          qvkDestroyDebugUtilsMessengerEXT;

//=============================================================================
// VMA allocator (extern, defined in vk_common.cpp)
//=============================================================================

#ifdef __cplusplus
extern VmaAllocator g_vmaAllocator;
#endif

//=============================================================================
// Structures
//=============================================================================

typedef struct vkSwapchain_s {
    VkSwapchainKHR          handle;
    VkFormat                format;
    VkColorSpaceKHR         colorSpace;
    VkExtent2D              extent;
    VkPresentModeKHR        presentMode;
    uint32_t                imageCount;
    VkImage                 images[VK_MAX_SWAPCHAIN_IMAGES];
    VkImageView             imageViews[VK_MAX_SWAPCHAIN_IMAGES];
} vkSwapchain_t;

typedef struct vkDepthBuffer_s {
    VkImage                 image;
    VkDeviceMemory          memory;
    VkImageView             view;
    VkFormat                format;
} vkDepthBuffer_t;

typedef struct vkFrame_s {
    VkCommandPool           commandPool;
    VkCommandBuffer         commandBuffer;
    VkSemaphore             imageAvailableSemaphore;
    VkSemaphore             renderFinishedSemaphore;
    VkFence                 inFlightFence;
    qboolean                commandBufferRecording;
    qboolean                inRenderPass;
} vkFrame_t;

typedef struct vkContext_s {
    // Vulkan library
    void*                   library;

    // Instance
    VkInstance              instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    // Physical device
    VkPhysicalDevice        physicalDevice;
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    // Logical device
    VkDevice                device;
    uint32_t                graphicsQueueFamily;
    uint32_t                presentQueueFamily;
    VkQueue                 graphicsQueue;
    VkQueue                 presentQueue;

    // Surface and swapchain
    VkSurfaceKHR            surface;
    vkSwapchain_t           swapchain;
    vkDepthBuffer_t         depthBuffer;

    // Render pass
    VkRenderPass            renderPass;
    VkFramebuffer           framebuffers[VK_MAX_SWAPCHAIN_IMAGES];

    // Per-frame resources
    vkFrame_t               frames[VK_MAX_FRAMES_IN_FLIGHT];
    uint32_t                currentFrame;
    uint32_t                currentImageIndex;

    // Pipeline cache
    VkPipelineCache         pipelineCache;

    // Descriptor pool
    VkDescriptorPool        descriptorPool;

    // Window handle (for recreation)
    HWND                    hwnd;

    // State
    qboolean                initialized;
    qboolean                inFrame;
    VkResult                lastError;
} vkContext_t;

extern vkContext_t vk;

//=============================================================================
// Functions
//=============================================================================

// Initialization
qboolean    Vk_LoadLibrary(void);
void        Vk_UnloadLibrary(void);
qboolean    Vk_LoadInstanceFunctions(void);
qboolean    Vk_LoadDeviceFunctions(void);

qboolean    Vk_CreateInstance(void);
void        Vk_DestroyInstance(void);

qboolean    Vk_CreateSurface(HWND hwnd);
void        Vk_DestroySurface(void);

qboolean    Vk_SelectPhysicalDevice(void);
qboolean    Vk_CreateDevice(void);
void        Vk_DestroyDevice(void);

qboolean    Vk_CreateSwapchain(void);
void        Vk_DestroySwapchain(void);
qboolean    Vk_RecreateSwapchain(void);

qboolean    Vk_CreateDepthBuffer(void);
void        Vk_DestroyDepthBuffer(void);

qboolean    Vk_CreateRenderPass(void);
void        Vk_DestroyRenderPass(void);

qboolean    Vk_CreateFramebuffers(void);
void        Vk_DestroyFramebuffers(void);

qboolean    Vk_CreateCommandPools(void);
void        Vk_DestroyCommandPools(void);

qboolean    Vk_CreateSyncObjects(void);
void        Vk_DestroySyncObjects(void);

qboolean    Vk_CreateDescriptorPool(void);
void        Vk_DestroyDescriptorPool(void);

qboolean    Vk_CreatePipelineCache(void);
void        Vk_DestroyPipelineCache(void);

qboolean    Vk_CreateVmaAllocator(void);
void        Vk_DestroyVmaAllocator(void);

// Utility
uint32_t    Vk_FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
VkFormat    Vk_FindDepthFormat(void);
VkFormat    Vk_FindSupportedFormat(const VkFormat* candidates, int candidateCount,
                                   VkImageTiling tiling, VkFormatFeatureFlags features);

// Command buffer helpers
VkCommandBuffer Vk_BeginSingleTimeCommands(void);
void            Vk_EndSingleTimeCommands(VkCommandBuffer commandBuffer);

// Image helpers
void        Vk_TransitionImageLayout(VkImage image, VkFormat format,
                                     VkImageLayout oldLayout, VkImageLayout newLayout,
                                     uint32_t mipLevels);
void        Vk_CopyBufferToImage(VkBuffer buffer, VkImage image,
                                 uint32_t width, uint32_t height);

// Error handling
const char* Vk_ResultString(VkResult result);

#ifdef __cplusplus
}
#endif

#endif // VK_COMMON_H
