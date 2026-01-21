#pragma once

#include "vk_common.h"
#include "vk_mem_alloc.h"
#include "vk_draw.h"

//----------------------------------------------------------------------------
// Device state structures
//----------------------------------------------------------------------------

struct vkQueueFamilyIndices_t {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    qboolean hasGraphicsFamily;
    qboolean hasPresentFamily;

    qboolean IsComplete() const {
        return hasGraphicsFamily && hasPresentFamily ? qtrue : qfalse;
    }
};

struct vkSwapchainSupportDetails_t {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct vkDeviceState_t {
    // Instance and device
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    // Queues
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    vkQueueFamilyIndices_t queueFamilyIndices;

    // Surface and swapchain
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainFormat;
    VkColorSpaceKHR swapchainColorSpace;
    VkExtent2D swapchainExtent;
    VkPresentModeKHR presentMode;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    uint32_t swapchainImageCount;

    // MSAA
    VkSampleCountFlagBits msaaSamples;
    VkImage msaaColorImage;
    VkImageView msaaColorView;
    VmaAllocation msaaColorAllocation;

    // Depth buffer
    VkImage depthImage;
    VkImageView depthImageView;
    VmaAllocation depthAllocation;
    VkFormat depthFormat;

    // Render pass and framebuffers
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    // Memory allocator
    VmaAllocator allocator;

    // Frame synchronization objects
    vkFrameData_t frames[VK_MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame;

    // Transfer command pool (separate from render pools to avoid conflicts)
    VkCommandPool transferCommandPool;

    // Debug messenger (debug builds only)
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT debugMessenger;
#endif
};

//----------------------------------------------------------------------------
// Global device state
//----------------------------------------------------------------------------
extern vkDeviceState_t g_vkDevice;

//----------------------------------------------------------------------------
// Device management functions
//----------------------------------------------------------------------------

// Initialize Vulkan instance
void VK_CreateInstance();

// Create debug messenger (debug builds)
void VK_SetupDebugMessenger();

// Select physical device
void VK_PickPhysicalDevice();

// Create logical device
void VK_CreateLogicalDevice();

// Create VMA allocator
void VK_CreateAllocator();

// Query queue families
vkQueueFamilyIndices_t VK_FindQueueFamilies(VkPhysicalDevice device);

// Query swapchain support
vkSwapchainSupportDetails_t VK_QuerySwapchainSupport(VkPhysicalDevice device);

// Check device suitability
qboolean VK_IsDeviceSuitable(VkPhysicalDevice device);

// Check device extension support
qboolean VK_CheckDeviceExtensionSupport(VkPhysicalDevice device);

// Get max usable sample count for MSAA
VkSampleCountFlagBits VK_GetMaxUsableSampleCount();

// Create swapchain
void VK_CreateSwapchain();

// Recreate swapchain (for resize, mode changes)
void VK_RecreateSwapchain();

// Create image views for swapchain
void VK_CreateImageViews();

// Create render pass
void VK_CreateRenderPass();

// Create depth resources
void VK_CreateDepthResources();

// Create MSAA resources
void VK_CreateMSAAResources();

// Create framebuffers
void VK_CreateFramebuffers();

// Create frame synchronization objects
void VK_CreateFrameSyncObjects();

// Find suitable depth format
VkFormat VK_FindDepthFormat();

// Find supported format
VkFormat VK_FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);

// Cleanup functions
void VK_CleanupSwapchain();
void VK_DestroyDevice();
void VK_DestroyInstance();

// Choose swap surface format
VkSurfaceFormatKHR VK_ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

// Choose swap present mode
VkPresentModeKHR VK_ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

// Choose swap extent
VkExtent2D VK_ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

