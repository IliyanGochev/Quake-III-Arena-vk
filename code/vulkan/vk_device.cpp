#include "vk_common.h"
#include "vk_state.h"
#include "vk_state.h"
#include "vk_image.h"

#include <vector>

//----------------------------------------------------------------------------
// Global Vulkan handles
//----------------------------------------------------------------------------

int        g_vkCurrentFrame = 0;
VkResult   g_vkLastError   = VK_SUCCESS;
uint32_t   g_vkCurrentImageIndex = 0;

VkInstance         g_vkInstance = VK_NULL_HANDLE;
VkPhysicalDevice   g_vkPhysicalDevice = VK_NULL_HANDLE;
VkDevice           g_vkDevice = VK_NULL_HANDLE;

VkQueue            g_vkGraphicsQueue = VK_NULL_HANDLE;
VkQueue            g_vkPresentQueue = VK_NULL_HANDLE;

VkSwapchainKHR     g_vkSwapchain = VK_NULL_HANDLE;
VkFormat           g_vkSwapchainFormat = VK_FORMAT_UNDEFINED;
uint32_t           g_vkSwapchainImageCount = 0;
VkImage*           g_vkSwapchainImages = nullptr;
VkImageView*       g_vkSwapchainImageViews = nullptr;

VkSemaphore        g_vkImageAvailableSemaphores[VK_MAX_FRAMES_IN_FLIGHT] = { };
VkSemaphore        g_vkRenderFinishedSemaphores[VK_MAX_FRAMES_IN_FLIGHT] = { };
VkFence            g_vkInFlightFences[VK_MAX_FRAMES_IN_FLIGHT] = { };

VkCommandPool      g_vkCommandPool = VK_NULL_HANDLE;
VkCommandBuffer    g_vkCommandBuffers[VK_MAX_FRAMES_IN_FLIGHT] = { };

VkCommandPool      g_vkTransferCommandPool = VK_NULL_HANDLE;
VkCommandBuffer    g_vkTransferCommandBuffer = VK_NULL_HANDLE;
VkFence            g_vkTransferFence = VK_NULL_HANDLE;

VkDescriptorPool   g_vkDescriptorPool = VK_NULL_HANDLE;
VkDescriptorSetLayout g_vkDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSet*   g_vkDescriptorSets = nullptr;

VkPipelineCache    g_vkPipelineCache = VK_NULL_HANDLE;

VkImage            g_vkDepthImage = VK_NULL_HANDLE;
VkImageView        g_vkDepthImageView = VK_NULL_HANDLE;
VmaAllocation      g_vkDepthAllocation = nullptr;
VkFormat           g_vkDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

VkRenderPass       g_vkRenderPass = VK_NULL_HANDLE;
VkFramebuffer*     g_vkFramebuffers = nullptr;

VmaAllocator       g_vkAllocator = nullptr;

VkSurfaceKHR       g_vkSurface = VK_NULL_HANDLE;

VkSampler          g_vkSamplerClamp = VK_NULL_HANDLE;
VkSampler          g_vkSamplerRepeat = VK_NULL_HANDLE;

VkPipelineLayout   g_vkPipelineLayout = VK_NULL_HANDLE;

//----------------------------------------------------------------------------
// Instance creation helpers
//----------------------------------------------------------------------------

VkPhysicalDeviceFeatures GetRequiredDeviceFeatures()
{
    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.wideLines = VK_TRUE;
    return features;
}

// Pre-surface device suitability: checks features and extensions only.
// Must be called BEFORE g_vkSurface is created (during device enumeration).
static VkBool32 IsDeviceSuitablePreSurface( VkPhysicalDevice device )
{
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties( device, &props );
    vkGetPhysicalDeviceFeatures( device, &features );

    // Discrete GPU preferred
    if ( props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
         props.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU )
        return VK_FALSE;

    // Check features
    if ( !features.samplerAnisotropy )
        return VK_FALSE;

    // Check required device extensions
    const char* const requiredExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME
    };
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties( device, nullptr, &extCount, nullptr );
    if ( extCount == 0 )
        return VK_FALSE;
    std::vector<VkExtensionProperties> extProps(extCount);
    vkEnumerateDeviceExtensionProperties( device, nullptr, &extCount, extProps.data() );

    for ( uint32_t i = 0; i < sizeof(requiredExtensions) / sizeof(requiredExtensions[0]); i++ )
    {
        VkBool32 found = VK_FALSE;
        for ( uint32_t j = 0; j < extCount; j++ )
        {
            if ( strcmp( requiredExtensions[i], extProps[j].extensionName ) == 0 )
            {
                found = VK_TRUE;
                break;
            }
        }
        if ( !found )
            return VK_FALSE;
    }

    return VK_TRUE;
}

// Post-surface device suitability: checks surface-related capabilities.
// Must be called AFTER g_vkSurface is created.
VkBool32 IsDeviceSuitablePostSurface( VkPhysicalDevice device )
{
    // Check swapchain format support
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR( device, g_vkSurface, &formatCount, nullptr );
    if ( formatCount == 0 )
        return VK_FALSE;

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR( device, g_vkSurface, &presentModeCount, nullptr );
    if ( presentModeCount == 0 )
        return VK_FALSE;

    // Check present support
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

    for ( uint32_t i = 0; i < queueFamilyCount; i++ )
    {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR( device, i, g_vkSurface, &presentSupport );
        if ( queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport )
            return VK_TRUE;
    }

    return VK_FALSE;
}

VkBool32 IsDeviceSuitable( VkPhysicalDevice device )
{
    // This function is kept for API compatibility.
    // Pre-surface checks happen during enumeration; post-surface checks
    // are done separately after surface creation.
    return IsDeviceSuitablePreSurface( device );
}

// Find a queue family that supports graphics + present.
// Must be called AFTER g_vkSurface is created.
uint32_t FindGraphicsAndPresentQueueFamily( VkPhysicalDevice device )
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

    for ( uint32_t i = 0; i < queueFamilyCount; i++ )
    {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR( device, i, g_vkSurface, &presentSupport );
        if ( queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport )
            return i;
    }
    ASSERT(0);
    return 0;
}

// Find a queue family that supports graphics only (no present check).
// Can be called BEFORE g_vkSurface is created.
static uint32_t FindGraphicsQueueFamily( VkPhysicalDevice device )
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

    for ( uint32_t i = 0; i < queueFamilyCount; i++ )
    {
        if ( queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
            return i;
    }
    ASSERT(0);
    return 0;
}

//----------------------------------------------------------------------------
// Swapchain creation
//----------------------------------------------------------------------------

static VkSurfaceFormatKHR SelectSurfaceFormat()
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR( g_vkPhysicalDevice, g_vkSurface, &formatCount, nullptr );
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR( g_vkPhysicalDevice, g_vkSurface, &formatCount, formats.data() );

    if ( formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED )
        return { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    // Prefer SRGB for proper color space handling
    for ( const auto& format : formats )
    {
        if ( format.format == VK_FORMAT_B8G8R8A8_SRGB &&
             format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
            return format;
    }
    // Fall back to UNORM
    for ( const auto& format : formats )
    {
        if ( format.format == VK_FORMAT_B8G8R8A8_UNORM &&
             format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
            return format;
    }

    return formats[0];
}

static VkPresentModeKHR SelectPresentMode()
{
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR( g_vkPhysicalDevice, g_vkSurface, &presentModeCount, nullptr );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR( g_vkPhysicalDevice, g_vkSurface, &presentModeCount, presentModes.data() );

    // Check vsync cvar: MAILBOX when vsync disabled, FIFO when enabled
    const cvar_t* vsyncCvar = ri.Cvar_Get( "r_vulkanVsync", "1", CVAR_ARCHIVE );
    qboolean wantVsync = (qboolean)(vsyncCvar && vsyncCvar->integer != 0);

    if ( wantVsync )
    {
        for ( const auto& mode : presentModes )
        {
            if ( mode == VK_PRESENT_MODE_FIFO_KHR )
                return mode;
        }
    }
    else
    {
        for ( const auto& mode : presentModes )
        {
            if ( mode == VK_PRESENT_MODE_MAILBOX_KHR )
                return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D SelectSwapchainExtent( uint32_t width, uint32_t height )
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( g_vkPhysicalDevice, g_vkSurface, &capabilities );

    if ( capabilities.currentExtent.width != 0xFFFFFFFF )
        return capabilities.currentExtent;

    return { width, height };
}

void VKDRV_CreateSwapchain( uint32_t width, uint32_t height )
{
    VkSurfaceFormatKHR surfaceFormat = SelectSurfaceFormat();
    VkPresentModeKHR presentMode = SelectPresentMode();
    VkExtent2D extent = SelectSwapchainExtent( width, height );

    g_vkSwapchainFormat = surfaceFormat.format;

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( g_vkPhysicalDevice, g_vkSurface, &capabilities );
    uint32_t imageCount = capabilities.minImageCount + 1;
    if ( capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount )
        imageCount = capabilities.maxImageCount;

    // Store old swapchain for efficient recreation
    VkSwapchainKHR oldSwapchain = g_vkSwapchain;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_vkSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VK_CHECK( vkCreateSwapchainKHR( g_vkDevice, &createInfo, nullptr, &g_vkSwapchain ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkSwapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain" );

    vkGetSwapchainImagesKHR( g_vkDevice, g_vkSwapchain, &g_vkSwapchainImageCount, nullptr );
    g_vkSwapchainImages = (VkImage*)malloc( g_vkSwapchainImageCount * sizeof(VkImage) );
    vkGetSwapchainImagesKHR( g_vkDevice, g_vkSwapchain, &g_vkSwapchainImageCount, g_vkSwapchainImages );

    g_vkSwapchainImageViews = (VkImageView*)calloc( g_vkSwapchainImageCount, sizeof(VkImageView) );
    for ( uint32_t i = 0; i < g_vkSwapchainImageCount; i++ )
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_vkSwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK( vkCreateImageView( g_vkDevice, &viewInfo, nullptr, &g_vkSwapchainImageViews[i] ) );
    }
}

void VKDRV_RecreateSwapchain( uint32_t width, uint32_t height )
{
    // Wait for GPU to idle before recreating swapchain
    vkDeviceWaitIdle( g_vkDevice );

    // Destroy dependent resources first (framebuffers, depth target)
    VKDRV_DestroyFramebuffers();
    VKDRV_DestroyDepthTarget();

    // Destroy and recreate swapchain
    VKDRV_DestroySwapchain();
    VKDRV_CreateSwapchain( width, height );

    // Recreate dependent resources
    VKDRV_CreateDepthTarget( width, height );
    VKDRV_CreateFramebuffers( g_vkSwapchainFormat, width, height );
}

void VKDRV_DestroySwapchain()
{
    if ( g_vkSwapchainImageViews )
    {
        for ( uint32_t i = 0; i < g_vkSwapchainImageCount; i++ )
            vkDestroyImageView( g_vkDevice, g_vkSwapchainImageViews[i], nullptr );
        free( g_vkSwapchainImageViews );
        g_vkSwapchainImageViews = nullptr;
    }
    if ( g_vkSwapchainImages )
    {
        free( g_vkSwapchainImages );
        g_vkSwapchainImages = nullptr;
    }
    if ( g_vkSwapchain )
    {
        vkDestroySwapchainKHR( g_vkDevice, g_vkSwapchain, nullptr );
        g_vkSwapchain = VK_NULL_HANDLE;
    }
    g_vkSwapchainImageCount = 0;
}

//----------------------------------------------------------------------------
// Depth/stencil buffer
//----------------------------------------------------------------------------

void VKDRV_CreateDepthTarget( uint32_t width, uint32_t height )
{
    VkFormat depthFormat = g_vkDepthFormat;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK( vmaCreateImage( g_vkAllocator, &imageInfo, &allocInfo,
                               &g_vkDepthImage, &g_vkDepthAllocation, nullptr ) );

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_vkDepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK( vkCreateImageView( g_vkDevice, &viewInfo, nullptr, &g_vkDepthImageView ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkDepthImage, VK_OBJECT_TYPE_IMAGE, "DepthImage" );
    VKDRV_SetDebugObjectName( (uint64_t)g_vkDepthImageView, VK_OBJECT_TYPE_IMAGE_VIEW, "DepthImageView" );
}

void VKDRV_DestroyDepthTarget()
{
    if ( g_vkDepthImageView )
    {
        vkDestroyImageView( g_vkDevice, g_vkDepthImageView, nullptr );
        g_vkDepthImageView = VK_NULL_HANDLE;
    }
    if ( g_vkDepthImage )
    {
        vmaDestroyImage( g_vkAllocator, g_vkDepthImage, g_vkDepthAllocation );
        g_vkDepthImage = VK_NULL_HANDLE;
        g_vkDepthAllocation = nullptr;
    }
}

//----------------------------------------------------------------------------
// Render pass & framebuffers
//----------------------------------------------------------------------------

void VKDRV_CreateRenderPass()
{
    // Color attachment (swapchain)
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = g_vkSwapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth/stencil attachment
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = g_vkDepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Subpass dependency for proper synchronization
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments = &colorAttachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VK_CHECK( vkCreateRenderPass( g_vkDevice, &info, nullptr, &g_vkRenderPass ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "MainRenderPass" );
}

void VKDRV_DestroyRenderPass()
{
    if ( g_vkRenderPass )
    {
        vkDestroyRenderPass( g_vkDevice, g_vkRenderPass, nullptr );
        g_vkRenderPass = VK_NULL_HANDLE;
    }
}

void VKDRV_CreateFramebuffers( VkFormat swapchainFormat, uint32_t width, uint32_t height )
{
    g_vkFramebuffers = (VkFramebuffer*)calloc( g_vkSwapchainImageCount, sizeof(VkFramebuffer) );

    for ( uint32_t i = 0; i < g_vkSwapchainImageCount; i++ )
    {
        VkImageView attachments[2] = {
            g_vkSwapchainImageViews[i],
            g_vkDepthImageView
        };

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = g_vkRenderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;

        VK_CHECK( vkCreateFramebuffer( g_vkDevice, &fbInfo, nullptr, &g_vkFramebuffers[i] ) );
    }
}

void VKDRV_DestroyFramebuffers()
{
    if ( g_vkFramebuffers )
    {
        for ( uint32_t i = 0; i < g_vkSwapchainImageCount; i++ )
            vkDestroyFramebuffer( g_vkDevice, g_vkFramebuffers[i], nullptr );
        free( g_vkFramebuffers );
        g_vkFramebuffers = nullptr;
    }
}

//----------------------------------------------------------------------------
// Command pool & command buffers
//----------------------------------------------------------------------------

void VKDRV_CreateCommandPool( uint32_t queueFamilyIndex )
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK( vkCreateCommandPool( g_vkDevice, &poolInfo, nullptr, &g_vkCommandPool ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL, "CommandPool" );

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_vkCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = VK_MAX_FRAMES_IN_FLIGHT;
    VK_CHECK( vkAllocateCommandBuffers( g_vkDevice, &allocInfo, g_vkCommandBuffers ) );

    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        char buf[32];
        snprintf( buf, sizeof(buf), "CommandBuffer%d", i );
        VKDRV_SetDebugObjectName( (uint64_t)g_vkCommandBuffers[i], VK_OBJECT_TYPE_COMMAND_BUFFER, buf );
    }
}

void VKDRV_DestroyCommandPool()
{
    if ( g_vkCommandPool )
    {
        vkDestroyCommandPool( g_vkDevice, g_vkCommandPool, nullptr );
        g_vkCommandPool = VK_NULL_HANDLE;
    }
}

void VKDRV_CreateTransferCommandBuffer( uint32_t queueFamilyIndex )
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK( vkCreateCommandPool( g_vkDevice, &poolInfo, nullptr, &g_vkTransferCommandPool ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkTransferCommandPool, VK_OBJECT_TYPE_COMMAND_POOL, "TransferCommandPool" );

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_vkTransferCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK( vkAllocateCommandBuffers( g_vkDevice, &allocInfo, &g_vkTransferCommandBuffer ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkTransferCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "TransferCommandBuffer" );
}

void VKDRV_DestroyTransferCommandBuffer()
{
    if ( g_vkTransferCommandBuffer )
    {
        vkFreeCommandBuffers( g_vkDevice, g_vkTransferCommandPool, 1, &g_vkTransferCommandBuffer );
        g_vkTransferCommandBuffer = VK_NULL_HANDLE;
    }
    if ( g_vkTransferCommandPool )
    {
        vkDestroyCommandPool( g_vkDevice, g_vkTransferCommandPool, nullptr );
        g_vkTransferCommandPool = VK_NULL_HANDLE;
    }
}

//----------------------------------------------------------------------------
// Semaphores & fences
//----------------------------------------------------------------------------

void VKDRV_CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        VK_CHECK( vkCreateSemaphore( g_vkDevice, &semaphoreInfo, nullptr, &g_vkImageAvailableSemaphores[i] ) );
        VK_CHECK( vkCreateSemaphore( g_vkDevice, &semaphoreInfo, nullptr, &g_vkRenderFinishedSemaphores[i] ) );
        VK_CHECK( vkCreateFence( g_vkDevice, &fenceInfo, nullptr, &g_vkInFlightFences[i] ) );
    }

    // Dedicated fence for transfer command buffer (avoids deadlock with render fence)
    VK_CHECK( vkCreateFence( g_vkDevice, &fenceInfo, nullptr, &g_vkTransferFence ) );
}

void VKDRV_DestroySyncObjects()
{
    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        vkDestroySemaphore( g_vkDevice, g_vkImageAvailableSemaphores[i], nullptr );
        vkDestroySemaphore( g_vkDevice, g_vkRenderFinishedSemaphores[i], nullptr );
        vkDestroyFence( g_vkDevice, g_vkInFlightFences[i], nullptr );
    }
    if ( g_vkTransferFence )
    {
        vkDestroyFence( g_vkDevice, g_vkTransferFence, nullptr );
        g_vkTransferFence = VK_NULL_HANDLE;
    }
}

//----------------------------------------------------------------------------
// Samplers
//----------------------------------------------------------------------------

static VkSampler CreateSampler( VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addrMode )
{
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = mipmapMode;
    info.addressModeU = addrMode;
    info.addressModeV = addrMode;
    info.addressModeW = addrMode;
    info.mipLodBias = 0.0f;

    // Use r_vulkanAnisotropy cvar for max anisotropy
    const cvar_t* anisoCvar = ri.Cvar_Get( "r_vulkanAnisotropy", "8", CVAR_ARCHIVE );
    float maxAniso = anisoCvar ? anisoCvar->integer : 8;
    if ( maxAniso < 1 ) maxAniso = 1;
    if ( maxAniso > 16 ) maxAniso = 16;
    info.anisotropyEnable = ( maxAniso > 1 ) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = maxAniso;

    info.compareEnable = VK_FALSE;
    info.minLod = 0.0f;
    info.maxLod = VK_LOD_CLAMP_NONE;

    VkSampler sampler;
    VK_CHECK( vkCreateSampler( g_vkDevice, &info, nullptr, &sampler ) );
    return sampler;
}

void VKDRV_CreateSamplers()
{
    g_vkSamplerClamp = CreateSampler( VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
    g_vkSamplerRepeat = CreateSampler( VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT );
}

void VKDRV_DestroySamplers()
{
    if ( g_vkSamplerClamp )
    {
        vkDestroySampler( g_vkDevice, g_vkSamplerClamp, nullptr );
        g_vkSamplerClamp = VK_NULL_HANDLE;
    }
    if ( g_vkSamplerRepeat )
    {
        vkDestroySampler( g_vkDevice, g_vkSamplerRepeat, nullptr );
        g_vkSamplerRepeat = VK_NULL_HANDLE;
    }
}

//----------------------------------------------------------------------------
// Descriptor pool & layout
//----------------------------------------------------------------------------

void VKDRV_CreateDescriptorSystem()
{
    // Descriptor set layout: 2 uniform buffers + 2 sampledd images
    VkDescriptorSetLayoutBinding bindings[VK_BIND_MAX] = {};

    bindings[VK_BIND_VIEW_VIEWPS].binding = VK_BIND_VIEW_VIEWPS;
    bindings[VK_BIND_VIEW_VIEWPS].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[VK_BIND_VIEW_VIEWPS].descriptorCount = 1;
    bindings[VK_BIND_VIEW_VIEWPS].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[VK_BIND_SKYBOX_VSEYE].binding = VK_BIND_SKYBOX_VSEYE;
    bindings[VK_BIND_SKYBOX_VSEYE].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[VK_BIND_SKYBOX_VSEYE].descriptorCount = 1;
    bindings[VK_BIND_SKYBOX_VSEYE].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[VK_BIND_SKYBOX_PSCOLOR].binding = VK_BIND_SKYBOX_PSCOLOR;
    bindings[VK_BIND_SKYBOX_PSCOLOR].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[VK_BIND_SKYBOX_PSCOLOR].descriptorCount = 1;
    bindings[VK_BIND_SKYBOX_PSCOLOR].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2 is shared between skybox PS color and quad color (drawn at different times)

    bindings[VK_BIND_TEXTURE_0].binding = VK_BIND_TEXTURE_0;
    bindings[VK_BIND_TEXTURE_0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[VK_BIND_TEXTURE_0].descriptorCount = 1;
    bindings[VK_BIND_TEXTURE_0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[VK_BIND_TEXTURE_1].binding = VK_BIND_TEXTURE_1;
    bindings[VK_BIND_TEXTURE_1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[VK_BIND_TEXTURE_1].descriptorCount = 1;
    bindings[VK_BIND_TEXTURE_1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = VK_BIND_MAX;
    layoutInfo.pBindings = bindings;
    VK_CHECK( vkCreateDescriptorSetLayout( g_vkDevice, &layoutInfo, nullptr, &g_vkDescriptorSetLayout ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "DescriptorSetLayout" );

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_vkDescriptorSetLayout;
    VK_CHECK( vkCreatePipelineLayout( g_vkDevice, &pipelineLayoutInfo, nullptr, &g_vkPipelineLayout ) );

    VKDRV_SetDebugObjectName( (uint64_t)g_vkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "PipelineLayout" );

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = VK_MAX_FRAMES_IN_FLIGHT * 16;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 256;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 512;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    VK_CHECK( vkCreateDescriptorPool( g_vkDevice, &poolInfo, nullptr, &g_vkDescriptorPool ) );

    // Allocate descriptor sets
    g_vkDescriptorSets = (VkDescriptorSet*)calloc( VK_MAX_FRAMES_IN_FLIGHT, sizeof(VkDescriptorSet) );
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_vkDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &g_vkDescriptorSetLayout;
    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        VK_CHECK( vkAllocateDescriptorSets( g_vkDevice, &allocInfo, &g_vkDescriptorSets[i] ) );
    }
}

void VKDRV_PopulateDescriptorSets()
{
    // Populate descriptor sets with uniform buffer references
    // This must be called after all uniform buffers are created (after InitDrawState)
    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        VkDescriptorBufferInfo vsBufInfo = {};
        vsBufInfo.buffer = g_vkDrawState.viewRenderData.vsUniformBuffer[i];
        vsBufInfo.offset = 0;
        vsBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo skyboxVsBufInfo = {};
        skyboxVsBufInfo.buffer = g_vkDrawState.skyBoxRenderData.vsUniformBuffer;
        skyboxVsBufInfo.offset = 0;
        skyboxVsBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo quadColorBufInfo = {};
        quadColorBufInfo.buffer = g_vkDrawState.quadRenderData.uniformBuffer;
        quadColorBufInfo.offset = 0;
        quadColorBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writes[3] = {};

        // Binding 0: view VS uniform buffer (projection, modelview matrices)
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = g_vkDescriptorSets[i];
        writes[0].dstBinding = VK_BIND_VIEW_VIEWPS;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &vsBufInfo;

        // Binding 1: skybox VS eye position buffer
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = g_vkDescriptorSets[i];
        writes[1].dstBinding = VK_BIND_SKYBOX_VSEYE;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &skyboxVsBufInfo;

        // Binding 2: quad color buffer (shared with skybox PS color at draw time)
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = g_vkDescriptorSets[i];
        writes[2].dstBinding = VK_BIND_SKYBOX_PSCOLOR;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo = &quadColorBufInfo;

        vkUpdateDescriptorSets( g_vkDevice, 3, writes, 0, nullptr );
    }

    // Initialize texture bindings (3, 4) with default white image
    const vkImage_t* defaultTex = GetImageRenderInfo( tr.whiteImage );
    if ( defaultTex && defaultTex->imageView )
    {
        VkDescriptorImageInfo texInfo = {};
        texInfo.sampler = defaultTex->sampler;
        texInfo.imageView = defaultTex->imageView;
        texInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
        {
            VkWriteDescriptorSet texWrites[2] = {};

            texWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrites[0].dstSet = g_vkDescriptorSets[i];
            texWrites[0].dstBinding = VK_BIND_TEXTURE_0;
            texWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            texWrites[0].descriptorCount = 1;
            texWrites[0].pImageInfo = &texInfo;

            texWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            texWrites[1].dstSet = g_vkDescriptorSets[i];
            texWrites[1].dstBinding = VK_BIND_TEXTURE_1;
            texWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            texWrites[1].descriptorCount = 1;
            texWrites[1].pImageInfo = &texInfo;

            vkUpdateDescriptorSets( g_vkDevice, 2, texWrites, 0, nullptr );
        }
    }
}

void VKDRV_UpdateTextureDescriptors( const vkImage_t* tex0, const vkImage_t* tex1 )
{
    VkDescriptorImageInfo info0 = {}, info1 = {};

    if ( tex0 && tex0->imageView )
    {
        info0.sampler = tex0->sampler;
        info0.imageView = tex0->imageView;
        info0.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( tex1 && tex1->imageView )
    {
        info1.sampler = tex1->sampler;
        info1.imageView = tex1->imageView;
        info1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[2] = {};
    int writeCount = 0;

    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = g_vkDescriptorSets[g_vkCurrentFrame];
    writes[writeCount].dstBinding = VK_BIND_TEXTURE_0;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &info0;
    writeCount++;

    if ( tex1 )
    {
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = g_vkDescriptorSets[g_vkCurrentFrame];
        writes[writeCount].dstBinding = VK_BIND_TEXTURE_1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].pImageInfo = &info1;
        writeCount++;
    }

    vkUpdateDescriptorSets( g_vkDevice, writeCount, writes, 0, nullptr );
}

void VKDRV_DestroyDescriptorSystem()
{
    if ( g_vkDescriptorSets )
    {
        free( g_vkDescriptorSets );
        g_vkDescriptorSets = nullptr;
    }
    if ( g_vkDescriptorPool )
    {
        vkDestroyDescriptorPool( g_vkDevice, g_vkDescriptorPool, nullptr );
        g_vkDescriptorPool = VK_NULL_HANDLE;
    }
    if ( g_vkDescriptorSetLayout )
    {
        vkDestroyDescriptorSetLayout( g_vkDevice, g_vkDescriptorSetLayout, nullptr );
        g_vkDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if ( g_vkPipelineLayout )
    {
        vkDestroyPipelineLayout( g_vkDevice, g_vkPipelineLayout, nullptr );
        g_vkPipelineLayout = VK_NULL_HANDLE;
    }
}

//----------------------------------------------------------------------------
// Begin/end single command buffer
//----------------------------------------------------------------------------

VkCommandBuffer VKDRV_BeginCommandBuffer()
{
    // Use the dedicated transfer command buffer to avoid resetting the
    // primary render command buffer (issue #6). This prevents ad-hoc
    // transfers from discarding in-flight render commands.
    VK_CHECK( vkResetCommandBuffer( g_vkTransferCommandBuffer, 0 ) );

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK( vkBeginCommandBuffer( g_vkTransferCommandBuffer, &beginInfo ) );
    return g_vkTransferCommandBuffer;
}

void VKDRV_EndCommandBuffer( VkCommandBuffer cmdBuffer )
{
    vkEndCommandBuffer( cmdBuffer );
}

void VKDRV_SubmitCommandBuffer( VkCommandBuffer cmdBuffer, qboolean wait, qboolean isTransfer )
{
    // Use the appropriate fence to avoid deadlock: transfer submissions must
    // NOT wait on the in-flight render fence, since the render fence can't
    // signal until the primary command buffer finishes, which would be blocked
    // waiting for the transfer to complete.
    VkFence fenceToUse = isTransfer ? g_vkTransferFence : g_vkInFlightFences[g_vkCurrentFrame];

    // Wait for the previous use of this fence before reusing it
    vkWaitForFences( g_vkDevice, 1, &fenceToUse, VK_TRUE, UINT64_MAX );
    vkResetFences( g_vkDevice, 1, &fenceToUse );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VK_CHECK( vkQueueSubmit( g_vkGraphicsQueue, 1, &submitInfo, fenceToUse ) );

    // When wait is true, block until the transfer completes before returning.
    // This is required for readback operations (ReadPixels, ReadDepth, ReadStencil)
    // to ensure the GPU has finished copying data before the CPU reads it.
    if ( wait )
    {
        vkWaitForFences( g_vkDevice, 1, &fenceToUse, VK_TRUE, UINT64_MAX );
    }
}

//----------------------------------------------------------------------------
// Swapchain image acquisition
//----------------------------------------------------------------------------

void VKDRV_BeginFrame()
{
    // Begin the primary command buffer for this frame
    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    VK_CHECK( vkResetCommandBuffer( cmd, 0 ) );

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // No ONE_TIME_SUBMIT_BIT for the primary render buffer
    beginInfo.flags = 0;
    VK_CHECK( vkBeginCommandBuffer( cmd, &beginInfo ) );
}

void VKDRV_AcquireNextImage()
{
    // Only acquire once per frame slot. Track the slot index that was last used.
    // -1 sentinel ensures first call always acquires; re-acquires after slot advances.
    static int lastAcquiredSlot = -1;
    if ( lastAcquiredSlot == g_vkCurrentFrame )
        return;
    lastAcquiredSlot = g_vkCurrentFrame;

    // Wait for this frame's fence before acquiring
    vkWaitForFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame], VK_TRUE, UINT64_MAX );
    vkResetFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame] );

    VkResult result = vkAcquireNextImageKHR(
        g_vkDevice, g_vkSwapchain, UINT64_MAX,
        g_vkImageAvailableSemaphores[g_vkCurrentFrame], VK_NULL_HANDLE,
        &g_vkCurrentImageIndex );

    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )
    {
        ri.Printf( PRINT_DEVELOPER, "WARNING: Swapchain out of date/suboptimal, recreating\n" );
        VKDRV_RecreateSwapchain( (uint32_t)vdConfig.vidWidth, (uint32_t)vdConfig.vidHeight );
        g_vkLastError = VK_SUCCESS;
    }
    else if ( result != VK_SUCCESS )
    {
        g_vkLastError = result;
    }
}

void VKDRV_SubmitAndPresent()
{
    // End debug label for render phase (issue #6)
    VKDRV_EndDebugLabel( g_vkCommandBuffers[g_vkCurrentFrame] );

    // End the render pass before submitting
    vkCmdEndRenderPass( g_vkCommandBuffers[g_vkCurrentFrame] );

    // End the primary command buffer before submitting
    vkEndCommandBuffer( g_vkCommandBuffers[g_vkCurrentFrame] );

    // Wait for this frame's fence (GPU may still be working on it from previous use)
    vkWaitForFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame], VK_TRUE, UINT64_MAX );
    vkResetFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame] );

    // Submit the primary command buffer with proper semaphore synchronization
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_vkCommandBuffers[g_vkCurrentFrame];

    VkSemaphore waitSemaphores[] = { g_vkImageAvailableSemaphores[g_vkCurrentFrame] };
    VkPipelineStageFlags waitStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStageMasks;

    VkSemaphore signalSemaphores[] = { g_vkRenderFinishedSemaphores[g_vkCurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK( vkQueueSubmit( g_vkGraphicsQueue, 1, &submitInfo, g_vkInFlightFences[g_vkCurrentFrame] ) );

    // Present the rendered image
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { g_vkSwapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &g_vkCurrentImageIndex;

    VkResult result = vkQueuePresentKHR( g_vkPresentQueue, &presentInfo );
    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )
    {
        ri.Printf( PRINT_DEVELOPER, "WARNING: Swapchain out of date/suboptimal during present, recreating\n" );
        VKDRV_RecreateSwapchain( (uint32_t)vdConfig.vidWidth, (uint32_t)vdConfig.vidHeight );
        g_vkLastError = VK_SUCCESS;
    }
    else if ( result != VK_SUCCESS )
    {
        g_vkLastError = result;
    }

    g_vkCurrentFrame = (g_vkCurrentFrame + 1) % VK_MAX_FRAMES_IN_FLIGHT;
}
