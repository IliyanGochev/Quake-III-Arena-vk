#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

#define VK_USE_PLATFORM_WIN32_KHR

// Forward declaration to break circular include (vk_state.h includes vk_common.h).
// Full definition is in vk_state.h, included by the .cpp files that need it.
struct vkImage_t;

extern "C" {
#   include "../renderer/tr_local.h"
#   include "../renderer/tr_layer.h"
#   include "../qcommon/qcommon.h"
}

#define VKDRV_PUBLIC  extern "C"

#ifndef __cplusplus
#   error This must only be included from C++ source files.
#endif

#include <assert.h>
#include <malloc.h>

#include <vulkan/vulkan.h>

// VMA (Vulkan Memory Allocator)
#include <vma/vk_mem_alloc.h>

#ifndef VK_CHECK
#   define VK_CHECK(x) { VkResult _vr = (x); if (_vr < 0) Com_Error( ERR_FATAL, "Vulkan Error %d: %s", _vr, #x ); }
#endif

#ifndef VK_CHECK_SOFT
#   define VK_CHECK_SOFT(x) { VkResult _vr = (x); if (_vr != VK_SUCCESS) g_vkLastError = _vr; }
#endif

#ifndef ASSERT
#   define ASSERT(x)	assert(x)
#endif

// Maximum number of in-flight frames
#define VK_MAX_FRAMES_IN_FLIGHT  2

// Maximum tessellation buffer size (matches D3D11 pattern)
#define VK_TESS_VERTEX_BUFFER_SIZE    (256 * 1024)
#define VK_TESS_INDEX_BUFFER_SIZE     (512 * 1024)
#define VK_TESS_STAGE_BUFFER_SIZE     (256 * 1024)

// Descriptor set layout bindings
enum VkDescriptorBinding {
    VK_BIND_VIEW_VIEWPS          = 0,
    VK_BIND_SKYBOX_VSEYE         = 1,
    VK_BIND_SKYBOX_PSCOLOR       = 2,  // shared with quad color (drawn at different times)
    VK_BIND_TEXTURE_0            = 3,
    VK_BIND_TEXTURE_1            = 4,
    VK_BIND_MAX                  = 5
};

// Per-frame resource indices
extern int        g_vkCurrentFrame;
extern VkResult   g_vkLastError;
extern uint32_t   g_vkCurrentImageIndex;

// Vulkan instance & physical device
extern VkInstance         g_vkInstance;
extern VkPhysicalDevice   g_vkPhysicalDevice;
extern VkDevice           g_vkDevice;

// Queues
extern VkQueue            g_vkGraphicsQueue;
extern VkQueue            g_vkPresentQueue;

// Swapchain
extern VkSwapchainKHR     g_vkSwapchain;
extern VkFormat           g_vkSwapchainFormat;
extern uint32_t           g_vkSwapchainImageCount;
extern VkImage*           g_vkSwapchainImages;
extern VkImageView*       g_vkSwapchainImageViews;

// Per-frame synchronization
extern VkSemaphore        g_vkImageAvailableSemaphores[VK_MAX_FRAMES_IN_FLIGHT];
extern VkSemaphore        g_vkRenderFinishedSemaphores[VK_MAX_FRAMES_IN_FLIGHT];
extern VkFence            g_vkInFlightFences[VK_MAX_FRAMES_IN_FLIGHT];

// Command buffers
extern VkCommandPool      g_vkCommandPool;
extern VkCommandBuffer    g_vkCommandBuffers[VK_MAX_FRAMES_IN_FLIGHT];

// Dedicated transfer command buffer (for ad-hoc transfers that must not
// reset the primary render command buffer during a frame)
extern VkCommandPool      g_vkTransferCommandPool;
extern VkCommandBuffer    g_vkTransferCommandBuffer;
extern VkFence            g_vkTransferFence;

// Descriptor pool & layout
extern VkDescriptorPool   g_vkDescriptorPool;
extern VkDescriptorSetLayout g_vkDescriptorSetLayout;
extern VkDescriptorSet*   g_vkDescriptorSets;

// Pipeline cache
extern VkPipelineCache    g_vkPipelineCache;

// Depth/stencil target
extern VkImage            g_vkDepthImage;
extern VkImageView        g_vkDepthImageView;
extern VmaAllocation      g_vkDepthAllocation;
extern VkFormat           g_vkDepthFormat;

// Render pass
extern VkRenderPass       g_vkRenderPass;
extern VkFramebuffer*     g_vkFramebuffers;

// VMA allocator
extern VmaAllocator       g_vkAllocator;

// Window surface
extern VkSurfaceKHR       g_vkSurface;

// Sampler cache
extern VkSampler          g_vkSamplerClamp;
extern VkSampler          g_vkSamplerRepeat;

// Gamma LUT (set by VKDrv_SetGamma, applied during texture upload)
extern unsigned char      g_vkGammaTable[256];

// Pipeline layout (defined in vk_state.cpp, needed by descriptor system)
extern VkPipelineLayout   g_vkPipelineLayout;

//----------------------------------------------------------------------------
// Internal device helper declarations (from vk_device.cpp)
//----------------------------------------------------------------------------

VkBool32 IsDeviceSuitable( VkPhysicalDevice device );
VkBool32 IsDeviceSuitablePostSurface( VkPhysicalDevice device );
uint32_t FindGraphicsAndPresentQueueFamily( VkPhysicalDevice device );
VkPhysicalDeviceFeatures GetRequiredDeviceFeatures();

void VKDRV_Init();
void VKDRV_Shutdown();

void VKDRV_CreateSwapchain( uint32_t width, uint32_t height );
void VKDRV_DestroySwapchain();
void VKDRV_RecreateSwapchain( uint32_t width, uint32_t height );

void VKDRV_CreateDepthTarget( uint32_t width, uint32_t height );
void VKDRV_DestroyDepthTarget();

void VKDRV_CreateRenderPass();
void VKDRV_DestroyRenderPass();

void VKDRV_CreateFramebuffers( VkFormat swapchainFormat, uint32_t width, uint32_t height );
void VKDRV_DestroyFramebuffers();

void VKDRV_CreateCommandPool( uint32_t queueFamilyIndex );
void VKDRV_DestroyCommandPool();

void VKDRV_CreateTransferCommandBuffer( uint32_t queueFamilyIndex );
void VKDRV_DestroyTransferCommandBuffer();

void VKDRV_CreateSyncObjects();
void VKDRV_DestroySyncObjects();

void VKDRV_CreateSamplers();
void VKDRV_DestroySamplers();

void VKDRV_CreateDescriptorSystem();
void VKDRV_DestroyDescriptorSystem();
void VKDRV_PopulateDescriptorSets();
void VKDRV_UpdateTextureDescriptors( const struct vkImage_t* tex0, const struct vkImage_t* tex1 );

void VKDRV_SetDebugObjectName( uint64_t object, VkObjectType type, const char* name );
void VKDRV_BeginDebugLabel( VkCommandBuffer cmd, const char* label );
void VKDRV_EndDebugLabel( VkCommandBuffer cmd );

void VKDRV_BeginFrame();
VkCommandBuffer VKDRV_BeginCommandBuffer();
void VKDRV_EndCommandBuffer( VkCommandBuffer cmdBuffer );
void VKDRV_SubmitCommandBuffer( VkCommandBuffer cmdBuffer, qboolean wait, qboolean isTransfer );
void VKDRV_AcquireNextImage();
void VKDRV_SubmitAndPresent();

#endif
