// @pjb: Vulkan rendering backend - common infrastructure implementation

// VMA implementation - must be defined before vk_common.h includes vk_mem_alloc.h
#define VMA_IMPLEMENTATION

#include "vk_common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//=============================================================================
// Global context
//=============================================================================

vkContext_t vk;

//=============================================================================
// Vulkan function pointers
//=============================================================================

// Instance-level functions
PFN_vkGetInstanceProcAddr                    qvkGetInstanceProcAddr = NULL;
PFN_vkCreateInstance                         qvkCreateInstance = NULL;
PFN_vkDestroyInstance                        qvkDestroyInstance = NULL;
PFN_vkEnumeratePhysicalDevices               qvkEnumeratePhysicalDevices = NULL;
PFN_vkGetPhysicalDeviceProperties            qvkGetPhysicalDeviceProperties = NULL;
PFN_vkGetPhysicalDeviceFeatures              qvkGetPhysicalDeviceFeatures = NULL;
PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
PFN_vkGetPhysicalDeviceMemoryProperties      qvkGetPhysicalDeviceMemoryProperties = NULL;
PFN_vkGetPhysicalDeviceFormatProperties      qvkGetPhysicalDeviceFormatProperties = NULL;
PFN_vkCreateDevice                           qvkCreateDevice = NULL;
PFN_vkDestroyDevice                          qvkDestroyDevice = NULL;
PFN_vkGetDeviceProcAddr                      qvkGetDeviceProcAddr = NULL;
PFN_vkGetDeviceQueue                         qvkGetDeviceQueue = NULL;

// Surface/swapchain functions
PFN_vkDestroySurfaceKHR                      qvkDestroySurfaceKHR = NULL;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR     qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR     qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
PFN_vkCreateSwapchainKHR                     qvkCreateSwapchainKHR = NULL;
PFN_vkDestroySwapchainKHR                    qvkDestroySwapchainKHR = NULL;
PFN_vkGetSwapchainImagesKHR                  qvkGetSwapchainImagesKHR = NULL;
PFN_vkAcquireNextImageKHR                    qvkAcquireNextImageKHR = NULL;
PFN_vkQueuePresentKHR                        qvkQueuePresentKHR = NULL;

// Win32 surface
PFN_vkCreateWin32SurfaceKHR                  qvkCreateWin32SurfaceKHR = NULL;

// Device-level functions
PFN_vkQueueSubmit                            qvkQueueSubmit = NULL;
PFN_vkQueueWaitIdle                          qvkQueueWaitIdle = NULL;
PFN_vkDeviceWaitIdle                         qvkDeviceWaitIdle = NULL;

// Memory
PFN_vkAllocateMemory                         qvkAllocateMemory = NULL;
PFN_vkFreeMemory                             qvkFreeMemory = NULL;
PFN_vkMapMemory                              qvkMapMemory = NULL;
PFN_vkUnmapMemory                            qvkUnmapMemory = NULL;
PFN_vkFlushMappedMemoryRanges                qvkFlushMappedMemoryRanges = NULL;
PFN_vkInvalidateMappedMemoryRanges           qvkInvalidateMappedMemoryRanges = NULL;
PFN_vkBindBufferMemory                       qvkBindBufferMemory = NULL;
PFN_vkBindImageMemory                        qvkBindImageMemory = NULL;
PFN_vkGetBufferMemoryRequirements            qvkGetBufferMemoryRequirements = NULL;
PFN_vkGetImageMemoryRequirements             qvkGetImageMemoryRequirements = NULL;

// Buffers
PFN_vkCreateBuffer                           qvkCreateBuffer = NULL;
PFN_vkDestroyBuffer                          qvkDestroyBuffer = NULL;

// Images
PFN_vkCreateImage                            qvkCreateImage = NULL;
PFN_vkDestroyImage                           qvkDestroyImage = NULL;
PFN_vkCreateImageView                        qvkCreateImageView = NULL;
PFN_vkDestroyImageView                       qvkDestroyImageView = NULL;
PFN_vkCreateSampler                          qvkCreateSampler = NULL;
PFN_vkDestroySampler                         qvkDestroySampler = NULL;

// Render passes and framebuffers
PFN_vkCreateRenderPass                       qvkCreateRenderPass = NULL;
PFN_vkDestroyRenderPass                      qvkDestroyRenderPass = NULL;
PFN_vkCreateFramebuffer                      qvkCreateFramebuffer = NULL;
PFN_vkDestroyFramebuffer                     qvkDestroyFramebuffer = NULL;

// Shaders and pipelines
PFN_vkCreateShaderModule                     qvkCreateShaderModule = NULL;
PFN_vkDestroyShaderModule                    qvkDestroyShaderModule = NULL;
PFN_vkCreatePipelineLayout                   qvkCreatePipelineLayout = NULL;
PFN_vkDestroyPipelineLayout                  qvkDestroyPipelineLayout = NULL;
PFN_vkCreateGraphicsPipelines                qvkCreateGraphicsPipelines = NULL;
PFN_vkDestroyPipeline                        qvkDestroyPipeline = NULL;
PFN_vkCreatePipelineCache                    qvkCreatePipelineCache = NULL;
PFN_vkDestroyPipelineCache                   qvkDestroyPipelineCache = NULL;
PFN_vkGetPipelineCacheData                   qvkGetPipelineCacheData = NULL;

// Descriptors
PFN_vkCreateDescriptorSetLayout              qvkCreateDescriptorSetLayout = NULL;
PFN_vkDestroyDescriptorSetLayout             qvkDestroyDescriptorSetLayout = NULL;
PFN_vkCreateDescriptorPool                   qvkCreateDescriptorPool = NULL;
PFN_vkDestroyDescriptorPool                  qvkDestroyDescriptorPool = NULL;
PFN_vkAllocateDescriptorSets                 qvkAllocateDescriptorSets = NULL;
PFN_vkFreeDescriptorSets                     qvkFreeDescriptorSets = NULL;
PFN_vkUpdateDescriptorSets                   qvkUpdateDescriptorSets = NULL;
PFN_vkResetDescriptorPool                    qvkResetDescriptorPool = NULL;

// Command pools and buffers
PFN_vkCreateCommandPool                      qvkCreateCommandPool = NULL;
PFN_vkDestroyCommandPool                     qvkDestroyCommandPool = NULL;
PFN_vkResetCommandPool                       qvkResetCommandPool = NULL;
PFN_vkAllocateCommandBuffers                 qvkAllocateCommandBuffers = NULL;
PFN_vkFreeCommandBuffers                     qvkFreeCommandBuffers = NULL;
PFN_vkBeginCommandBuffer                     qvkBeginCommandBuffer = NULL;
PFN_vkEndCommandBuffer                       qvkEndCommandBuffer = NULL;
PFN_vkResetCommandBuffer                     qvkResetCommandBuffer = NULL;

// Command buffer commands
PFN_vkCmdBeginRenderPass                     qvkCmdBeginRenderPass = NULL;
PFN_vkCmdEndRenderPass                       qvkCmdEndRenderPass = NULL;
PFN_vkCmdBindPipeline                        qvkCmdBindPipeline = NULL;
PFN_vkCmdBindDescriptorSets                  qvkCmdBindDescriptorSets = NULL;
PFN_vkCmdBindVertexBuffers                   qvkCmdBindVertexBuffers = NULL;
PFN_vkCmdBindIndexBuffer                     qvkCmdBindIndexBuffer = NULL;
PFN_vkCmdSetViewport                         qvkCmdSetViewport = NULL;
PFN_vkCmdSetScissor                          qvkCmdSetScissor = NULL;
PFN_vkCmdSetDepthBias                        qvkCmdSetDepthBias = NULL;
PFN_vkCmdSetBlendConstants                   qvkCmdSetBlendConstants = NULL;
PFN_vkCmdDraw                                qvkCmdDraw = NULL;
PFN_vkCmdDrawIndexed                         qvkCmdDrawIndexed = NULL;
PFN_vkCmdCopyBuffer                          qvkCmdCopyBuffer = NULL;
PFN_vkCmdCopyBufferToImage                   qvkCmdCopyBufferToImage = NULL;
PFN_vkCmdCopyImage                           qvkCmdCopyImage = NULL;
PFN_vkCmdBlitImage                           qvkCmdBlitImage = NULL;
PFN_vkCmdClearColorImage                     qvkCmdClearColorImage = NULL;
PFN_vkCmdClearDepthStencilImage              qvkCmdClearDepthStencilImage = NULL;
PFN_vkCmdClearAttachments                    qvkCmdClearAttachments = NULL;
PFN_vkCmdPipelineBarrier                     qvkCmdPipelineBarrier = NULL;
PFN_vkCmdPushConstants                       qvkCmdPushConstants = NULL;

// Synchronization
PFN_vkCreateSemaphore                        qvkCreateSemaphore = NULL;
PFN_vkDestroySemaphore                       qvkDestroySemaphore = NULL;
PFN_vkCreateFence                            qvkCreateFence = NULL;
PFN_vkDestroyFence                           qvkDestroyFence = NULL;
PFN_vkWaitForFences                          qvkWaitForFences = NULL;
PFN_vkResetFences                            qvkResetFences = NULL;
PFN_vkGetFenceStatus                         qvkGetFenceStatus = NULL;

// Debug (optional)
PFN_vkCreateDebugUtilsMessengerEXT           qvkCreateDebugUtilsMessengerEXT = NULL;
PFN_vkDestroyDebugUtilsMessengerEXT          qvkDestroyDebugUtilsMessengerEXT = NULL;

//=============================================================================
// VMA allocator
//=============================================================================

VmaAllocator g_vmaAllocator = VK_NULL_HANDLE;

// Depth buffer allocation (stored separately since vkDepthBuffer_t is in C header)
static VmaAllocation s_depthBufferAllocation = VK_NULL_HANDLE;

//=============================================================================
// Library loading
//=============================================================================

qboolean Vk_LoadLibrary(void)
{
    Com_Printf("Loading vulkan-1.dll...\n");

    vk.library = LoadLibraryA("vulkan-1.dll");
    if (!vk.library) {
        Com_Printf("ERROR: Failed to load vulkan-1.dll\n");
        return qfalse;
    }

    qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(
        (HMODULE)vk.library, "vkGetInstanceProcAddr");
    if (!qvkGetInstanceProcAddr) {
        Com_Printf("ERROR: Failed to get vkGetInstanceProcAddr\n");
        Vk_UnloadLibrary();
        return qfalse;
    }

    // Get pre-instance functions
    qvkCreateInstance = (PFN_vkCreateInstance)qvkGetInstanceProcAddr(NULL, "vkCreateInstance");

    if (!qvkCreateInstance) {
        Com_Printf("ERROR: Failed to get vkCreateInstance\n");
        Vk_UnloadLibrary();
        return qfalse;
    }

    return qtrue;
}

void Vk_UnloadLibrary(void)
{
    if (vk.library) {
        FreeLibrary((HMODULE)vk.library);
        vk.library = NULL;
    }

    qvkGetInstanceProcAddr = NULL;
    qvkCreateInstance = NULL;
}

qboolean Vk_LoadInstanceFunctions(void)
{
    if (!vk.instance) {
        return qfalse;
    }

#define VK_INSTANCE_FUNC(func) \
    q##func = (PFN_##func)qvkGetInstanceProcAddr(vk.instance, #func); \
    if (!q##func) { \
        Com_Printf("ERROR: Failed to get " #func "\n"); \
        return qfalse; \
    }

    VK_INSTANCE_FUNC(vkDestroyInstance);
    VK_INSTANCE_FUNC(vkEnumeratePhysicalDevices);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceProperties);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceFormatProperties);
    VK_INSTANCE_FUNC(vkCreateDevice);
    VK_INSTANCE_FUNC(vkDestroyDevice);
    VK_INSTANCE_FUNC(vkGetDeviceProcAddr);
    VK_INSTANCE_FUNC(vkGetDeviceQueue);

    // Surface functions
    VK_INSTANCE_FUNC(vkDestroySurfaceKHR);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);
    VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR);

    // Win32 surface
    VK_INSTANCE_FUNC(vkCreateWin32SurfaceKHR);

    // Debug (optional - don't fail if missing)
    qvkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
        qvkGetInstanceProcAddr(vk.instance, "vkCreateDebugUtilsMessengerEXT");
    qvkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
        qvkGetInstanceProcAddr(vk.instance, "vkDestroyDebugUtilsMessengerEXT");

#undef VK_INSTANCE_FUNC

    return qtrue;
}

qboolean Vk_LoadDeviceFunctions(void)
{
    if (!vk.device) {
        return qfalse;
    }

#define VK_DEVICE_FUNC(func) \
    q##func = (PFN_##func)qvkGetDeviceProcAddr(vk.device, #func); \
    if (!q##func) { \
        Com_Printf("ERROR: Failed to get " #func "\n"); \
        return qfalse; \
    }

    // Swapchain
    VK_DEVICE_FUNC(vkCreateSwapchainKHR);
    VK_DEVICE_FUNC(vkDestroySwapchainKHR);
    VK_DEVICE_FUNC(vkGetSwapchainImagesKHR);
    VK_DEVICE_FUNC(vkAcquireNextImageKHR);
    VK_DEVICE_FUNC(vkQueuePresentKHR);

    // Queue
    VK_DEVICE_FUNC(vkQueueSubmit);
    VK_DEVICE_FUNC(vkQueueWaitIdle);
    VK_DEVICE_FUNC(vkDeviceWaitIdle);

    // Memory
    VK_DEVICE_FUNC(vkAllocateMemory);
    VK_DEVICE_FUNC(vkFreeMemory);
    VK_DEVICE_FUNC(vkMapMemory);
    VK_DEVICE_FUNC(vkUnmapMemory);
    VK_DEVICE_FUNC(vkFlushMappedMemoryRanges);
    VK_DEVICE_FUNC(vkInvalidateMappedMemoryRanges);
    VK_DEVICE_FUNC(vkBindBufferMemory);
    VK_DEVICE_FUNC(vkBindImageMemory);
    VK_DEVICE_FUNC(vkGetBufferMemoryRequirements);
    VK_DEVICE_FUNC(vkGetImageMemoryRequirements);

    // Buffers
    VK_DEVICE_FUNC(vkCreateBuffer);
    VK_DEVICE_FUNC(vkDestroyBuffer);

    // Images
    VK_DEVICE_FUNC(vkCreateImage);
    VK_DEVICE_FUNC(vkDestroyImage);
    VK_DEVICE_FUNC(vkCreateImageView);
    VK_DEVICE_FUNC(vkDestroyImageView);
    VK_DEVICE_FUNC(vkCreateSampler);
    VK_DEVICE_FUNC(vkDestroySampler);

    // Render passes and framebuffers
    VK_DEVICE_FUNC(vkCreateRenderPass);
    VK_DEVICE_FUNC(vkDestroyRenderPass);
    VK_DEVICE_FUNC(vkCreateFramebuffer);
    VK_DEVICE_FUNC(vkDestroyFramebuffer);

    // Shaders and pipelines
    VK_DEVICE_FUNC(vkCreateShaderModule);
    VK_DEVICE_FUNC(vkDestroyShaderModule);
    VK_DEVICE_FUNC(vkCreatePipelineLayout);
    VK_DEVICE_FUNC(vkDestroyPipelineLayout);
    VK_DEVICE_FUNC(vkCreateGraphicsPipelines);
    VK_DEVICE_FUNC(vkDestroyPipeline);
    VK_DEVICE_FUNC(vkCreatePipelineCache);
    VK_DEVICE_FUNC(vkDestroyPipelineCache);
    VK_DEVICE_FUNC(vkGetPipelineCacheData);

    // Descriptors
    VK_DEVICE_FUNC(vkCreateDescriptorSetLayout);
    VK_DEVICE_FUNC(vkDestroyDescriptorSetLayout);
    VK_DEVICE_FUNC(vkCreateDescriptorPool);
    VK_DEVICE_FUNC(vkDestroyDescriptorPool);
    VK_DEVICE_FUNC(vkAllocateDescriptorSets);
    VK_DEVICE_FUNC(vkFreeDescriptorSets);
    VK_DEVICE_FUNC(vkUpdateDescriptorSets);
    VK_DEVICE_FUNC(vkResetDescriptorPool);

    // Command pools and buffers
    VK_DEVICE_FUNC(vkCreateCommandPool);
    VK_DEVICE_FUNC(vkDestroyCommandPool);
    VK_DEVICE_FUNC(vkResetCommandPool);
    VK_DEVICE_FUNC(vkAllocateCommandBuffers);
    VK_DEVICE_FUNC(vkFreeCommandBuffers);
    VK_DEVICE_FUNC(vkBeginCommandBuffer);
    VK_DEVICE_FUNC(vkEndCommandBuffer);
    VK_DEVICE_FUNC(vkResetCommandBuffer);

    // Command buffer commands
    VK_DEVICE_FUNC(vkCmdBeginRenderPass);
    VK_DEVICE_FUNC(vkCmdEndRenderPass);
    VK_DEVICE_FUNC(vkCmdBindPipeline);
    VK_DEVICE_FUNC(vkCmdBindDescriptorSets);
    VK_DEVICE_FUNC(vkCmdBindVertexBuffers);
    VK_DEVICE_FUNC(vkCmdBindIndexBuffer);
    VK_DEVICE_FUNC(vkCmdSetViewport);
    VK_DEVICE_FUNC(vkCmdSetScissor);
    VK_DEVICE_FUNC(vkCmdSetDepthBias);
    VK_DEVICE_FUNC(vkCmdSetBlendConstants);
    VK_DEVICE_FUNC(vkCmdDraw);
    VK_DEVICE_FUNC(vkCmdDrawIndexed);
    VK_DEVICE_FUNC(vkCmdCopyBuffer);
    VK_DEVICE_FUNC(vkCmdCopyBufferToImage);
    VK_DEVICE_FUNC(vkCmdCopyImage);
    VK_DEVICE_FUNC(vkCmdBlitImage);
    VK_DEVICE_FUNC(vkCmdClearColorImage);
    VK_DEVICE_FUNC(vkCmdClearDepthStencilImage);
    VK_DEVICE_FUNC(vkCmdClearAttachments);
    VK_DEVICE_FUNC(vkCmdPipelineBarrier);
    VK_DEVICE_FUNC(vkCmdPushConstants);

    // Synchronization
    VK_DEVICE_FUNC(vkCreateSemaphore);
    VK_DEVICE_FUNC(vkDestroySemaphore);
    VK_DEVICE_FUNC(vkCreateFence);
    VK_DEVICE_FUNC(vkDestroyFence);
    VK_DEVICE_FUNC(vkWaitForFences);
    VK_DEVICE_FUNC(vkResetFences);
    VK_DEVICE_FUNC(vkGetFenceStatus);

#undef VK_DEVICE_FUNC

    return qtrue;
}

//=============================================================================
// Instance creation
//=============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL Vk_DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    const char* severity = "UNKNOWN";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity = "ERROR";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity = "WARNING";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severity = "INFO";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        severity = "VERBOSE";

    Com_Printf("Vulkan [%s]: %s\n", severity, pCallbackData->pMessage);

    return VK_FALSE;
}

qboolean Vk_CreateInstance(void)
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Quake III Arena";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "id Tech 3";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifdef _DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    const char* layers[] = {
#ifdef _DEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.ppEnabledExtensionNames = extensions;
#ifdef _DEBUG
    createInfo.enabledLayerCount = sizeof(layers) / sizeof(layers[0]);
    createInfo.ppEnabledLayerNames = layers;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
#endif

    VkResult result = qvkCreateInstance(&createInfo, NULL, &vk.instance);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateInstance failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    Com_Printf("Vulkan instance created\n");

    // Load instance functions
    if (!Vk_LoadInstanceFunctions()) {
        Vk_DestroyInstance();
        return qfalse;
    }

#ifdef _DEBUG
    // Setup debug messenger
    if (qvkCreateDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = Vk_DebugCallback;

        qvkCreateDebugUtilsMessengerEXT(vk.instance, &debugInfo, NULL, &vk.debugMessenger);
    }
#endif

    return qtrue;
}

void Vk_DestroyInstance(void)
{
#ifdef _DEBUG
    if (vk.debugMessenger && qvkDestroyDebugUtilsMessengerEXT) {
        qvkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debugMessenger, NULL);
        vk.debugMessenger = VK_NULL_HANDLE;
    }
#endif

    if (vk.instance) {
        qvkDestroyInstance(vk.instance, NULL);
        vk.instance = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Surface
//=============================================================================

qboolean Vk_CreateSurface(HWND hwnd)
{
    vk.hwnd = hwnd;

    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(NULL);
    createInfo.hwnd = hwnd;

    VkResult result = qvkCreateWin32SurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateWin32SurfaceKHR failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    Com_Printf("Vulkan surface created\n");
    return qtrue;
}

void Vk_DestroySurface(void)
{
    if (vk.surface) {
        qvkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
        vk.surface = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Physical device selection
//=============================================================================

qboolean Vk_SelectPhysicalDevice(void)
{
    uint32_t deviceCount = 0;
    qvkEnumeratePhysicalDevices(vk.instance, &deviceCount, NULL);

    if (deviceCount == 0) {
        Com_Printf("ERROR: No Vulkan-capable devices found\n");
        return qfalse;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)Z_Malloc(deviceCount * sizeof(VkPhysicalDevice));
    qvkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices);

    // Prefer discrete GPU
    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties props;
        qvkGetPhysicalDeviceProperties(devices[i], &props);

        // Check for required queue families
        uint32_t queueFamilyCount = 0;
        qvkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, NULL);

        VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)
            Z_Malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
        qvkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, queueFamilies);

        int graphicsFamily = -1;
        int presentFamily = -1;

        for (uint32_t j = 0; j < queueFamilyCount; j++) {
            if (queueFamilies[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily = j;
            }

            VkBool32 presentSupport = VK_FALSE;
            qvkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, vk.surface, &presentSupport);
            if (presentSupport) {
                presentFamily = j;
            }

            if (graphicsFamily >= 0 && presentFamily >= 0) {
                break;
            }
        }

        Z_Free(queueFamilies);

        if (graphicsFamily >= 0 && presentFamily >= 0) {
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                selectedDevice = devices[i];
                vk.graphicsQueueFamily = graphicsFamily;
                vk.presentQueueFamily = presentFamily;
                break;
            } else if (selectedDevice == VK_NULL_HANDLE) {
                selectedDevice = devices[i];
                vk.graphicsQueueFamily = graphicsFamily;
                vk.presentQueueFamily = presentFamily;
            }
        }
    }

    Z_Free(devices);

    if (selectedDevice == VK_NULL_HANDLE) {
        Com_Printf("ERROR: No suitable Vulkan device found\n");
        return qfalse;
    }

    vk.physicalDevice = selectedDevice;
    qvkGetPhysicalDeviceProperties(vk.physicalDevice, &vk.deviceProperties);
    qvkGetPhysicalDeviceFeatures(vk.physicalDevice, &vk.deviceFeatures);
    qvkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &vk.memoryProperties);

    Com_Printf("Vulkan device: %s\n", vk.deviceProperties.deviceName);
    Com_Printf("  API version: %d.%d.%d\n",
        VK_VERSION_MAJOR(vk.deviceProperties.apiVersion),
        VK_VERSION_MINOR(vk.deviceProperties.apiVersion),
        VK_VERSION_PATCH(vk.deviceProperties.apiVersion));

    return qtrue;
}

//=============================================================================
// Logical device
//=============================================================================

qboolean Vk_CreateDevice(void)
{
    float queuePriority = 1.0f;

    // Create queue create infos
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    uint32_t queueCreateInfoCount = 1;

    queueCreateInfos[0] = {};
    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = vk.graphicsQueueFamily;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;

    if (vk.graphicsQueueFamily != vk.presentQueueFamily) {
        queueCreateInfos[1] = {};
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].queueFamilyIndex = vk.presentQueueFamily;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = &queuePriority;
        queueCreateInfoCount = 2;
    }

    // Required device extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Device features we want
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = vk.deviceFeatures.samplerAnisotropy;
    deviceFeatures.fillModeNonSolid = vk.deviceFeatures.fillModeNonSolid;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.pEnabledFeatures = &deviceFeatures;

    VkResult result = qvkCreateDevice(vk.physicalDevice, &createInfo, NULL, &vk.device);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateDevice failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    // Load device functions
    if (!Vk_LoadDeviceFunctions()) {
        Vk_DestroyDevice();
        return qfalse;
    }

    // Get queues
    qvkGetDeviceQueue(vk.device, vk.graphicsQueueFamily, 0, &vk.graphicsQueue);
    qvkGetDeviceQueue(vk.device, vk.presentQueueFamily, 0, &vk.presentQueue);

    // Initialize VMA - provide all required Vulkan function pointers
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = qvkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = qvkGetDeviceProcAddr;
    vulkanFunctions.vkGetPhysicalDeviceProperties = qvkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = qvkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = qvkAllocateMemory;
    vulkanFunctions.vkFreeMemory = qvkFreeMemory;
    vulkanFunctions.vkMapMemory = qvkMapMemory;
    vulkanFunctions.vkUnmapMemory = qvkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = qvkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = qvkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = qvkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = qvkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = qvkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = qvkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = qvkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = qvkDestroyBuffer;
    vulkanFunctions.vkCreateImage = qvkCreateImage;
    vulkanFunctions.vkDestroyImage = qvkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = qvkCmdCopyBuffer;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    allocatorInfo.physicalDevice = vk.physicalDevice;
    allocatorInfo.device = vk.device;
    allocatorInfo.instance = vk.instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    result = vmaCreateAllocator(&allocatorInfo, &g_vmaAllocator);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vmaCreateAllocator failed: %s\n", Vk_ResultString(result));
        Vk_DestroyDevice();
        return qfalse;
    }

    Com_Printf("Vulkan device created\n");
    return qtrue;
}

void Vk_DestroyDevice(void)
{
    if (g_vmaAllocator) {
        vmaDestroyAllocator(g_vmaAllocator);
        g_vmaAllocator = VK_NULL_HANDLE;
    }

    if (vk.device) {
        qvkDestroyDevice(vk.device, NULL);
        vk.device = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Swapchain
//=============================================================================

qboolean Vk_CreateSwapchain(void)
{
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &capabilities);

    // Choose surface format
    uint32_t formatCount;
    qvkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, NULL);
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)Z_Malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    qvkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, formats);

    vk.swapchain.format = formats[0].format;
    vk.swapchain.colorSpace = formats[0].colorSpace;

    // Prefer SRGB
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            vk.swapchain.format = formats[i].format;
            vk.swapchain.colorSpace = formats[i].colorSpace;
            break;
        }
    }
    Z_Free(formats);

    // Choose present mode
    uint32_t presentModeCount;
    qvkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, NULL);
    VkPresentModeKHR* presentModes = (VkPresentModeKHR*)Z_Malloc(presentModeCount * sizeof(VkPresentModeKHR));
    qvkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, presentModes);

    vk.swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available

    // Prefer mailbox (triple buffering)
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            vk.swapchain.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    Z_Free(presentModes);

    // Choose extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        vk.swapchain.extent = capabilities.currentExtent;
    } else {
        RECT rect;
        GetClientRect(vk.hwnd, &rect);
        vk.swapchain.extent.width = rect.right - rect.left;
        vk.swapchain.extent.height = rect.bottom - rect.top;
    }

    // Choose image count
    vk.swapchain.imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && vk.swapchain.imageCount > capabilities.maxImageCount) {
        vk.swapchain.imageCount = capabilities.maxImageCount;
    }
    if (vk.swapchain.imageCount > VK_MAX_SWAPCHAIN_IMAGES) {
        vk.swapchain.imageCount = VK_MAX_SWAPCHAIN_IMAGES;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vk.surface;
    createInfo.minImageCount = vk.swapchain.imageCount;
    createInfo.imageFormat = vk.swapchain.format;
    createInfo.imageColorSpace = vk.swapchain.colorSpace;
    createInfo.imageExtent = vk.swapchain.extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { vk.graphicsQueueFamily, vk.presentQueueFamily };
    if (vk.graphicsQueueFamily != vk.presentQueueFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = vk.swapchain.presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = qvkCreateSwapchainKHR(vk.device, &createInfo, NULL, &vk.swapchain.handle);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateSwapchainKHR failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    // Get swapchain images
    qvkGetSwapchainImagesKHR(vk.device, vk.swapchain.handle, &vk.swapchain.imageCount, NULL);
    qvkGetSwapchainImagesKHR(vk.device, vk.swapchain.handle, &vk.swapchain.imageCount, vk.swapchain.images);

    // Create image views
    for (uint32_t i = 0; i < vk.swapchain.imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk.swapchain.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vk.swapchain.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = qvkCreateImageView(vk.device, &viewInfo, NULL, &vk.swapchain.imageViews[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("ERROR: vkCreateImageView failed: %s\n", Vk_ResultString(result));
            Vk_DestroySwapchain();
            return qfalse;
        }
    }

    Com_Printf("Vulkan swapchain created: %dx%d, %d images\n",
        vk.swapchain.extent.width, vk.swapchain.extent.height, vk.swapchain.imageCount);

    return qtrue;
}

void Vk_DestroySwapchain(void)
{
    for (uint32_t i = 0; i < vk.swapchain.imageCount; i++) {
        if (vk.swapchain.imageViews[i]) {
            qvkDestroyImageView(vk.device, vk.swapchain.imageViews[i], NULL);
            vk.swapchain.imageViews[i] = VK_NULL_HANDLE;
        }
    }

    if (vk.swapchain.handle) {
        qvkDestroySwapchainKHR(vk.device, vk.swapchain.handle, NULL);
        vk.swapchain.handle = VK_NULL_HANDLE;
    }
}

qboolean Vk_RecreateSwapchain(void)
{
    qvkDeviceWaitIdle(vk.device);

    Vk_DestroyFramebuffers();
    Vk_DestroyDepthBuffer();
    Vk_DestroySwapchain();

    if (!Vk_CreateSwapchain()) return qfalse;
    if (!Vk_CreateDepthBuffer()) return qfalse;
    if (!Vk_CreateFramebuffers()) return qfalse;

    return qtrue;
}

//=============================================================================
// Depth buffer
//=============================================================================

qboolean Vk_CreateDepthBuffer(void)
{
    vk.depthBuffer.format = Vk_FindDepthFormat();

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = vk.swapchain.extent.width;
    imageInfo.extent.height = vk.swapchain.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk.depthBuffer.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(g_vmaAllocator, &imageInfo, &allocInfo,
        &vk.depthBuffer.image, &s_depthBufferAllocation, NULL);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vmaCreateImage failed for depth buffer: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk.depthBuffer.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vk.depthBuffer.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = qvkCreateImageView(vk.device, &viewInfo, NULL, &vk.depthBuffer.view);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateImageView failed for depth buffer: %s\n", Vk_ResultString(result));
        Vk_DestroyDepthBuffer();
        return qfalse;
    }

    return qtrue;
}

void Vk_DestroyDepthBuffer(void)
{
    if (vk.depthBuffer.view) {
        qvkDestroyImageView(vk.device, vk.depthBuffer.view, NULL);
        vk.depthBuffer.view = VK_NULL_HANDLE;
    }

    if (vk.depthBuffer.image) {
        vmaDestroyImage(g_vmaAllocator, vk.depthBuffer.image, s_depthBufferAllocation);
        vk.depthBuffer.image = VK_NULL_HANDLE;
        s_depthBufferAllocation = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Render pass
//=============================================================================

qboolean Vk_CreateRenderPass(void)
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = vk.swapchain.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = vk.depthBuffer.format;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    VkResult result = qvkCreateRenderPass(vk.device, &createInfo, NULL, &vk.renderPass);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateRenderPass failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    return qtrue;
}

void Vk_DestroyRenderPass(void)
{
    if (vk.renderPass) {
        qvkDestroyRenderPass(vk.device, vk.renderPass, NULL);
        vk.renderPass = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Framebuffers
//=============================================================================

qboolean Vk_CreateFramebuffers(void)
{
    for (uint32_t i = 0; i < vk.swapchain.imageCount; i++) {
        VkImageView attachments[] = {
            vk.swapchain.imageViews[i],
            vk.depthBuffer.view
        };

        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = vk.renderPass;
        createInfo.attachmentCount = 2;
        createInfo.pAttachments = attachments;
        createInfo.width = vk.swapchain.extent.width;
        createInfo.height = vk.swapchain.extent.height;
        createInfo.layers = 1;

        VkResult result = qvkCreateFramebuffer(vk.device, &createInfo, NULL, &vk.framebuffers[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("ERROR: vkCreateFramebuffer failed: %s\n", Vk_ResultString(result));
            Vk_DestroyFramebuffers();
            return qfalse;
        }
    }

    return qtrue;
}

void Vk_DestroyFramebuffers(void)
{
    for (uint32_t i = 0; i < vk.swapchain.imageCount; i++) {
        if (vk.framebuffers[i]) {
            qvkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);
            vk.framebuffers[i] = VK_NULL_HANDLE;
        }
    }
}

//=============================================================================
// Command pools
//=============================================================================

qboolean Vk_CreateCommandPools(void)
{
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = vk.graphicsQueueFamily;

        VkResult result = qvkCreateCommandPool(vk.device, &poolInfo, NULL, &vk.frames[i].commandPool);
        if (result != VK_SUCCESS) {
            Com_Printf("ERROR: vkCreateCommandPool failed: %s\n", Vk_ResultString(result));
            Vk_DestroyCommandPools();
            return qfalse;
        }

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vk.frames[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        result = qvkAllocateCommandBuffers(vk.device, &allocInfo, &vk.frames[i].commandBuffer);
        if (result != VK_SUCCESS) {
            Com_Printf("ERROR: vkAllocateCommandBuffers failed: %s\n", Vk_ResultString(result));
            Vk_DestroyCommandPools();
            return qfalse;
        }
    }

    return qtrue;
}

void Vk_DestroyCommandPools(void)
{
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vk.frames[i].commandPool) {
            qvkDestroyCommandPool(vk.device, vk.frames[i].commandPool, NULL);
            vk.frames[i].commandPool = VK_NULL_HANDLE;
            vk.frames[i].commandBuffer = VK_NULL_HANDLE;
        }
    }
}

//=============================================================================
// Synchronization
//=============================================================================

qboolean Vk_CreateSyncObjects(void)
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult result = qvkCreateSemaphore(vk.device, &semaphoreInfo, NULL,
            &vk.frames[i].imageAvailableSemaphore);
        if (result != VK_SUCCESS) {
            Vk_DestroySyncObjects();
            return qfalse;
        }

        result = qvkCreateSemaphore(vk.device, &semaphoreInfo, NULL,
            &vk.frames[i].renderFinishedSemaphore);
        if (result != VK_SUCCESS) {
            Vk_DestroySyncObjects();
            return qfalse;
        }

        result = qvkCreateFence(vk.device, &fenceInfo, NULL,
            &vk.frames[i].inFlightFence);
        if (result != VK_SUCCESS) {
            Vk_DestroySyncObjects();
            return qfalse;
        }
    }

    return qtrue;
}

void Vk_DestroySyncObjects(void)
{
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vk.frames[i].imageAvailableSemaphore) {
            qvkDestroySemaphore(vk.device, vk.frames[i].imageAvailableSemaphore, NULL);
            vk.frames[i].imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (vk.frames[i].renderFinishedSemaphore) {
            qvkDestroySemaphore(vk.device, vk.frames[i].renderFinishedSemaphore, NULL);
            vk.frames[i].renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (vk.frames[i].inFlightFence) {
            qvkDestroyFence(vk.device, vk.frames[i].inFlightFence, NULL);
            vk.frames[i].inFlightFence = VK_NULL_HANDLE;
        }
    }
}

//=============================================================================
// Descriptor pool
//=============================================================================

qboolean Vk_CreateDescriptorPool(void)
{
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },  // Diffuse + Lightmap textures
        { VK_DESCRIPTOR_TYPE_SAMPLER, 4096 },
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = VK_MAX_DESCRIPTOR_SETS;

    VkResult result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &vk.descriptorPool);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateDescriptorPool failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    return qtrue;
}

void Vk_DestroyDescriptorPool(void)
{
    if (vk.descriptorPool) {
        qvkDestroyDescriptorPool(vk.device, vk.descriptorPool, NULL);
        vk.descriptorPool = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Pipeline cache
//=============================================================================

qboolean Vk_CreatePipelineCache(void)
{
    VkPipelineCacheCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    // TODO: Load from disk if available

    VkResult result = qvkCreatePipelineCache(vk.device, &createInfo, NULL, &vk.pipelineCache);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreatePipelineCache failed: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    return qtrue;
}

void Vk_DestroyPipelineCache(void)
{
    // TODO: Save to disk

    if (vk.pipelineCache) {
        qvkDestroyPipelineCache(vk.device, vk.pipelineCache, NULL);
        vk.pipelineCache = VK_NULL_HANDLE;
    }
}

//=============================================================================
// Utility functions
//=============================================================================

uint32_t Vk_FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < vk.memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (vk.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    Com_Printf("ERROR: Failed to find suitable memory type\n");
    return 0;
}

VkFormat Vk_FindDepthFormat(void)
{
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    return Vk_FindSupportedFormat(candidates, 3,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat Vk_FindSupportedFormat(const VkFormat* candidates, int candidateCount,
                                VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (int i = 0; i < candidateCount; i++) {
        VkFormatProperties props;
        qvkGetPhysicalDeviceFormatProperties(vk.physicalDevice, candidates[i], &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return candidates[i];
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features) {
            return candidates[i];
        }
    }

    Com_Printf("ERROR: Failed to find supported format\n");
    return VK_FORMAT_UNDEFINED;
}

VkCommandBuffer Vk_BeginSingleTimeCommands(void)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vk.frames[vk.currentFrame].commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    qvkAllocateCommandBuffers(vk.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    qvkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Vk_EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    qvkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    qvkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    qvkQueueWaitIdle(vk.graphicsQueue);

    qvkFreeCommandBuffers(vk.device, vk.frames[vk.currentFrame].commandPool, 1, &commandBuffer);
}

void Vk_TransitionImageLayout(VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout,
                              uint32_t mipLevels)
{
    VkCommandBuffer commandBuffer = Vk_BeginSingleTimeCommands();

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

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    qvkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
        0, NULL, 0, NULL, 1, &barrier);

    Vk_EndSingleTimeCommands(commandBuffer);
}

void Vk_CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = Vk_BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    qvkCmdCopyBufferToImage(commandBuffer, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    Vk_EndSingleTimeCommands(commandBuffer);
}

const char* Vk_ResultString(VkResult result)
{
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        default: return "UNKNOWN_ERROR";
    }
}
