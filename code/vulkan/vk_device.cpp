#include "vk_device.h"
#include "vk_driver.h"
#include "../win32/win_vk.h"
#include <set>

// Global device state
vkDeviceState_t g_vkDevice = {};

// Required device extensions
static const char* deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static const uint32_t deviceExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);

// Validation layers
#ifdef _DEBUG
static const char* validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const uint32_t validationLayerCount = sizeof(validationLayers) / sizeof(validationLayers[0]);
static qboolean enableValidationLayers = qtrue;

// Forward declaration of debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);
#else
static qboolean enableValidationLayers = qfalse;
#endif

//----------------------------------------------------------------------------
// Check if validation layers are available
//----------------------------------------------------------------------------
static qboolean CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

#ifdef _DEBUG
    for (const char* layerName : validationLayers) {
        qboolean layerFound = qfalse;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = qtrue;
                break;
            }
        }

        if (!layerFound) {
            return qfalse;
        }
    }
#endif

    return qtrue;
}

//----------------------------------------------------------------------------
// Get required instance extensions
//----------------------------------------------------------------------------
static std::vector<const char*> GetRequiredExtensions() {
    std::vector<const char*> extensions;

    // Platform-specific surface extension
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    // Debug utils extension for validation layers
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

//----------------------------------------------------------------------------
// Create Vulkan instance
//----------------------------------------------------------------------------
void VK_CreateInstance() {
    VK_LOG("Creating Vulkan instance...\n");

    // Check validation layer support
    if (enableValidationLayers && !CheckValidationLayerSupport()) {
        ri.Printf(PRINT_WARNING, "Vulkan validation layers requested, but not available. Disabling.\n");
        enableValidationLayers = qfalse;
    }

    // Application info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Quake III Arena";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "id Tech 3";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    // Instance create info
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get required extensions
    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Enable validation layers if requested
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = validationLayerCount;
        createInfo.ppEnabledLayerNames = validationLayers;

        // Setup debug messenger create info for instance creation/destruction
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;

        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // Create instance
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &g_vkDevice.instance));

    VK_LOG("Vulkan instance created successfully\n");
    if (enableValidationLayers) {
        VK_LOG("Validation layers enabled\n");
    }
}

//----------------------------------------------------------------------------
// Debug callback for validation layers
//----------------------------------------------------------------------------
#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    // Filter out verbose info messages
    if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        return VK_FALSE;
    }

    // Print based on severity
    const char* severityStr = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severityStr = "ERROR";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severityStr = "WARNING";
    }

    ri.Printf(PRINT_ALL, "VK %s: %s\n", severityStr, pCallbackData->pMessage);

    return VK_FALSE;
}
#endif

//----------------------------------------------------------------------------
// Setup debug messenger
//----------------------------------------------------------------------------
void VK_SetupDebugMessenger() {
#ifdef _DEBUG
    if (!enableValidationLayers) {
        return;
    }

    VK_LOG("Setting up debug messenger...\n");

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;

    // Load the extension function
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        g_vkDevice.instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        VK_CHECK(func(g_vkDevice.instance, &createInfo, nullptr, &g_vkDevice.debugMessenger));
        VK_LOG("Debug messenger created successfully\n");
    } else {
        ri.Error(ERR_FATAL, "Failed to load vkCreateDebugUtilsMessengerEXT\n");
    }
#endif
}

//----------------------------------------------------------------------------
// Destroy debug messenger
//----------------------------------------------------------------------------
static void DestroyDebugMessenger() {
#ifdef _DEBUG
    if (g_vkDevice.debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            g_vkDevice.instance, "vkDestroyDebugUtilsMessengerEXT");

        if (func != nullptr) {
            func(g_vkDevice.instance, g_vkDevice.debugMessenger, nullptr);
            g_vkDevice.debugMessenger = VK_NULL_HANDLE;
            VK_LOG("Debug messenger destroyed\n");
        }
    }
#endif
}

//----------------------------------------------------------------------------
// Destroy instance
//----------------------------------------------------------------------------
void VK_DestroyInstance() {
    DestroyDebugMessenger();

    if (g_vkDevice.instance) {
        vkDestroyInstance(g_vkDevice.instance, nullptr);
        g_vkDevice.instance = VK_NULL_HANDLE;
        VK_LOG("Vulkan instance destroyed\n");
    }
}

//----------------------------------------------------------------------------
// Find queue families
//----------------------------------------------------------------------------
vkQueueFamilyIndices_t VK_FindQueueFamilies(VkPhysicalDevice device) {
    vkQueueFamilyIndices_t indices = {};
    indices.hasGraphicsFamily = qfalse;
    indices.hasPresentFamily = qfalse;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Check for graphics support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.hasGraphicsFamily = qtrue;
        }

        // Check for present support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_vkDevice.surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
            indices.hasPresentFamily = qtrue;
        }

        if (indices.IsComplete()) {
            break;
        }
    }

    return indices;
}

//----------------------------------------------------------------------------
// Check device extension support
//----------------------------------------------------------------------------
qboolean VK_CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (uint32_t i = 0; i < deviceExtensionCount; i++) {
        qboolean found = qfalse;
        for (const auto& extension : availableExtensions) {
            if (strcmp(deviceExtensions[i], extension.extensionName) == 0) {
                found = qtrue;
                break;
            }
        }
        if (!found) {
            return qfalse;
        }
    }

    return qtrue;
}

//----------------------------------------------------------------------------
// Query swapchain support
//----------------------------------------------------------------------------
vkSwapchainSupportDetails_t VK_QuerySwapchainSupport(VkPhysicalDevice device) {
    vkSwapchainSupportDetails_t details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, g_vkDevice.surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vkDevice.surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vkDevice.surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vkDevice.surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vkDevice.surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

//----------------------------------------------------------------------------
// Check if device is suitable
//----------------------------------------------------------------------------
qboolean VK_IsDeviceSuitable(VkPhysicalDevice device) {
    // Check queue families
    vkQueueFamilyIndices_t indices = VK_FindQueueFamilies(device);
    if (!indices.IsComplete()) {
        return qfalse;
    }

    // Check extension support
    if (!VK_CheckDeviceExtensionSupport(device)) {
        return qfalse;
    }

    // Check swapchain support
    vkSwapchainSupportDetails_t swapchainSupport = VK_QuerySwapchainSupport(device);
    if (swapchainSupport.formats.empty() || swapchainSupport.presentModes.empty()) {
        return qfalse;
    }

    // Check for required features
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // We need sampler anisotropy
    if (!supportedFeatures.samplerAnisotropy) {
        return qfalse;
    }

    return qtrue;
}

//----------------------------------------------------------------------------
// Rate device suitability (higher score is better)
//----------------------------------------------------------------------------
static int RateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum texture size affects quality
    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

//----------------------------------------------------------------------------
// Pick physical device
//----------------------------------------------------------------------------
void VK_PickPhysicalDevice() {
    VK_LOG("Selecting physical device...\n");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_vkDevice.instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        ri.Error(ERR_FATAL, "Failed to find GPUs with Vulkan support\n");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_vkDevice.instance, &deviceCount, devices.data());

    // Find best suitable device
    int bestScore = -1;
    for (const auto& device : devices) {
        if (VK_IsDeviceSuitable(device)) {
            int score = RateDeviceSuitability(device);
            if (score > bestScore) {
                g_vkDevice.physicalDevice = device;
                bestScore = score;
            }
        }
    }

    if (g_vkDevice.physicalDevice == VK_NULL_HANDLE) {
        ri.Error(ERR_FATAL, "Failed to find a suitable GPU\n");
    }

    // Get device properties and features
    vkGetPhysicalDeviceProperties(g_vkDevice.physicalDevice, &g_vkDevice.deviceProperties);
    vkGetPhysicalDeviceFeatures(g_vkDevice.physicalDevice, &g_vkDevice.deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(g_vkDevice.physicalDevice, &g_vkDevice.memoryProperties);

    VK_LOG("Selected GPU: %s\n", g_vkDevice.deviceProperties.deviceName);
    VK_LOG("Vulkan API Version: %d.%d.%d\n",
           VK_VERSION_MAJOR(g_vkDevice.deviceProperties.apiVersion),
           VK_VERSION_MINOR(g_vkDevice.deviceProperties.apiVersion),
           VK_VERSION_PATCH(g_vkDevice.deviceProperties.apiVersion));
}

//----------------------------------------------------------------------------
// Create logical device
//----------------------------------------------------------------------------
void VK_CreateLogicalDevice() {
    VK_LOG("Creating logical device...\n");

    g_vkDevice.queueFamilyIndices = VK_FindQueueFamilies(g_vkDevice.physicalDevice);

    // Create queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        g_vkDevice.queueFamilyIndices.graphicsFamily,
        g_vkDevice.queueFamilyIndices.presentFamily
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Specify device features
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // For wireframe mode

    // Create logical device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = deviceExtensionCount;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    // Validation layers (deprecated for devices, but for compatibility)
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = validationLayerCount;
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VK_CHECK(vkCreateDevice(g_vkDevice.physicalDevice, &createInfo, nullptr, &g_vkDevice.device));

    // Get queue handles
    vkGetDeviceQueue(g_vkDevice.device, g_vkDevice.queueFamilyIndices.graphicsFamily, 0, &g_vkDevice.graphicsQueue);
    vkGetDeviceQueue(g_vkDevice.device, g_vkDevice.queueFamilyIndices.presentFamily, 0, &g_vkDevice.presentQueue);

    VK_LOG("Logical device created successfully\n");
    VK_LOG("Graphics queue family: %d\n", g_vkDevice.queueFamilyIndices.graphicsFamily);
    VK_LOG("Present queue family: %d\n", g_vkDevice.queueFamilyIndices.presentFamily);
}

//----------------------------------------------------------------------------
// Create VMA allocator
//----------------------------------------------------------------------------
void VK_CreateAllocator() {
    VK_LOG("Creating VMA allocator...\n");

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = g_vkDevice.physicalDevice;
    allocatorInfo.device = g_vkDevice.device;
    allocatorInfo.instance = g_vkDevice.instance;

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &g_vkDevice.allocator));

    VK_LOG("VMA allocator created successfully\n");
}

//----------------------------------------------------------------------------
// Destroy VMA allocator
//----------------------------------------------------------------------------
static void DestroyAllocator() {
    if (g_vkDevice.allocator) {
        vmaDestroyAllocator(g_vkDevice.allocator);
        g_vkDevice.allocator = VK_NULL_HANDLE;
        VK_LOG("VMA allocator destroyed\n");
    }
}

//----------------------------------------------------------------------------
// Get maximum usable MSAA sample count
//----------------------------------------------------------------------------
VkSampleCountFlagBits VK_GetMaxUsableSampleCount() {
    VkSampleCountFlags counts = g_vkDevice.deviceProperties.limits.framebufferColorSampleCounts &
                                 g_vkDevice.deviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

//----------------------------------------------------------------------------
// Find supported format from candidates
//----------------------------------------------------------------------------
VkFormat VK_FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(g_vkDevice.physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    ri.Error(ERR_FATAL, "Failed to find supported format\n");
    return VK_FORMAT_UNDEFINED;
}

//----------------------------------------------------------------------------
// Find depth format
//----------------------------------------------------------------------------
VkFormat VK_FindDepthFormat() {
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT
    };

    return VK_FindSupportedFormat(
        candidates,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

//----------------------------------------------------------------------------
// Choose best surface format (prefer B8G8R8A8_UNORM or SRGB)
//----------------------------------------------------------------------------
VkSurfaceFormatKHR VK_ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer B8G8R8A8_UNORM with SRGB_NONLINEAR color space
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // If not available, try B8G8R8A8_SRGB
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Fall back to first available format
    return availableFormats[0];
}

//----------------------------------------------------------------------------
// Choose present mode (FIFO for vsync, IMMEDIATE for no vsync)
//----------------------------------------------------------------------------
VkPresentModeKHR VK_ChoosePresentMode(const std::vector<VkPresentModeKHR>& availableModes, qboolean vsync) {
    if (!vsync) {
        // Try to use IMMEDIATE mode (no vsync)
        for (const auto& mode : availableModes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }

        // Try MAILBOX as second choice (triple buffering)
        for (const auto& mode : availableModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
    }

    // FIFO is guaranteed to be available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

//----------------------------------------------------------------------------
// Choose swap extent (match window size)
//----------------------------------------------------------------------------
VkExtent2D VK_ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // If width is UINT32_MAX, we can choose our own extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        ri.Printf(PRINT_ALL, "...using surface capabilities extent: %dx%d\n",
                  capabilities.currentExtent.width, capabilities.currentExtent.height);
        return capabilities.currentExtent;
    }

    // Otherwise, get window size and clamp to supported range
    int width, height;
    VKWnd_GetWindowSize(&width, &height);

    ri.Printf(PRINT_ALL, "...surface extent is flexible, using window size: %dx%d\n", width, height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = (std::max)(capabilities.minImageExtent.width,
                                   (std::min)(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = (std::max)(capabilities.minImageExtent.height,
                                    (std::min)(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
}

//----------------------------------------------------------------------------
// Create swapchain
//----------------------------------------------------------------------------
void VK_CreateSwapchain() {
    // Query swapchain support
    vkSwapchainSupportDetails_t swapchainSupport = VK_QuerySwapchainSupport(g_vkDevice.physicalDevice);

    // Choose settings
    VkSurfaceFormatKHR surfaceFormat = VK_ChooseSurfaceFormat(swapchainSupport.formats);

    cvar_t* vk_vsync = ri.Cvar_Get("vk_vsync", "1", CVAR_ARCHIVE);
    VkPresentModeKHR presentMode = VK_ChoosePresentMode(swapchainSupport.presentModes, (vk_vsync->integer != 0) ? qtrue : qfalse);

    VkExtent2D extent = VK_ChooseSwapExtent(swapchainSupport.capabilities);

    // Image count (prefer 3 for triple buffering)
    uint32_t imageCount = 3;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }
    if (imageCount < swapchainSupport.capabilities.minImageCount) {
        imageCount = swapchainSupport.capabilities.minImageCount;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_vkDevice.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Queue family indices
    vkQueueFamilyIndices_t indices = VK_FindQueueFamilies(g_vkDevice.physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(g_vkDevice.device, &createInfo, nullptr, &g_vkDevice.swapchain));

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(g_vkDevice.device, g_vkDevice.swapchain, &g_vkDevice.swapchainImageCount, nullptr);
    g_vkDevice.swapchainImages.resize(g_vkDevice.swapchainImageCount);
    vkGetSwapchainImagesKHR(g_vkDevice.device, g_vkDevice.swapchain, &g_vkDevice.swapchainImageCount, g_vkDevice.swapchainImages.data());

    g_vkDevice.swapchainFormat = surfaceFormat.format;
    g_vkDevice.swapchainExtent = extent;

    ri.Printf(PRINT_ALL, "...created swapchain with extent: %dx%d\n", extent.width, extent.height);

    // Create image views
    g_vkDevice.swapchainImageViews.resize(g_vkDevice.swapchainImageCount);
    for (size_t i = 0; i < g_vkDevice.swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_vkDevice.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = g_vkDevice.swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(g_vkDevice.device, &viewInfo, nullptr, &g_vkDevice.swapchainImageViews[i]));
    }

    // Get MSAA sample count
    cvar_t* vk_multisamples = ri.Cvar_Get("vk_multisamples", "1", CVAR_ARCHIVE | CVAR_LATCH);
    VkSampleCountFlagBits requestedSamples = (VkSampleCountFlagBits)vk_multisamples->integer;
    VkSampleCountFlagBits maxSamples = VK_GetMaxUsableSampleCount();

    g_vkDevice.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    if (requestedSamples > VK_SAMPLE_COUNT_1_BIT && requestedSamples <= maxSamples) {
        g_vkDevice.msaaSamples = requestedSamples;
    }

    // Create MSAA color image if needed
    if (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = g_vkDevice.swapchainExtent.width;
        imageInfo.extent.height = g_vkDevice.swapchainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = g_vkDevice.swapchainFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = g_vkDevice.msaaSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VK_CHECK(vmaCreateImage(g_vkDevice.allocator, &imageInfo, &allocInfo,
            &g_vkDevice.msaaColorImage, &g_vkDevice.msaaColorAllocation, nullptr));

        // Create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_vkDevice.msaaColorImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = g_vkDevice.swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(g_vkDevice.device, &viewInfo, nullptr, &g_vkDevice.msaaColorView));
    }

    // Create depth buffer
    VkFormat depthFormat = VK_FindDepthFormat();

    VkImageCreateInfo depthImageInfo = {};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.extent.width = g_vkDevice.swapchainExtent.width;
    depthImageInfo.extent.height = g_vkDevice.swapchainExtent.height;
    depthImageInfo.extent.depth = 1;
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.format = depthFormat;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.samples = g_vkDevice.msaaSamples;  // Match MSAA samples
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo depthAllocInfo = {};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    depthAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VK_CHECK(vmaCreateImage(g_vkDevice.allocator, &depthImageInfo, &depthAllocInfo,
        &g_vkDevice.depthImage, &g_vkDevice.depthAllocation, nullptr));

    // Create depth image view
    VkImageViewCreateInfo depthViewInfo = {};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = g_vkDevice.depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = depthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(g_vkDevice.device, &depthViewInfo, nullptr, &g_vkDevice.depthImageView));

    g_vkDevice.depthFormat = depthFormat;
}

//----------------------------------------------------------------------------
// Create render pass
//----------------------------------------------------------------------------
void VK_CreateRenderPass() {
    qboolean msaaEnabled = (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? qtrue : qfalse;

    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentReference colorAttachmentRef = {};
    VkAttachmentReference resolveAttachmentRef = {};
    VkAttachmentReference depthAttachmentRef = {};

    if (msaaEnabled) {
        // MSAA render pass: 3 attachments
        // [0] MSAA color attachment
        // [1] Resolve attachment (swapchain image)
        // [2] Depth/stencil attachment

        // MSAA color attachment
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = g_vkDevice.swapchainFormat;
        colorAttachment.samples = g_vkDevice.msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Resolve attachment (swapchain image)
        VkAttachmentDescription resolveAttachment = {};
        resolveAttachment.format = g_vkDevice.swapchainFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = g_vkDevice.depthFormat;
        depthAttachment.samples = g_vkDevice.msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments = { colorAttachment, resolveAttachment, depthAttachment };

        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        resolveAttachmentRef.attachment = 1;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
        // Non-MSAA render pass: 2 attachments
        // [0] Color attachment (swapchain image)
        // [1] Depth/stencil attachment

        // Color attachment (swapchain image)
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = g_vkDevice.swapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = g_vkDevice.depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments = { colorAttachment, depthAttachment };

        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    if (msaaEnabled) {
        subpass.pResolveAttachments = &resolveAttachmentRef;
    }

    // Subpass dependency
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(g_vkDevice.device, &renderPassInfo, nullptr, &g_vkDevice.renderPass));
}

//----------------------------------------------------------------------------
// Create framebuffers
//----------------------------------------------------------------------------
void VK_CreateFramebuffers() {
    qboolean msaaEnabled = (g_vkDevice.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? qtrue : qfalse;

    g_vkDevice.framebuffers.resize(g_vkDevice.swapchainImageCount);

    for (size_t i = 0; i < g_vkDevice.swapchainImageCount; i++) {
        std::vector<VkImageView> attachments;

        if (msaaEnabled) {
            // MSAA framebuffer: 3 attachments
            // [0] MSAA color image (shared)
            // [1] Swapchain image view (resolve target)
            // [2] Depth image (shared)
            attachments = {
                g_vkDevice.msaaColorView,
                g_vkDevice.swapchainImageViews[i],
                g_vkDevice.depthImageView
            };
        } else {
            // Non-MSAA framebuffer: 2 attachments
            // [0] Swapchain image view
            // [1] Depth image (shared)
            attachments = {
                g_vkDevice.swapchainImageViews[i],
                g_vkDevice.depthImageView
            };
        }

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_vkDevice.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = g_vkDevice.swapchainExtent.width;
        framebufferInfo.height = g_vkDevice.swapchainExtent.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(g_vkDevice.device, &framebufferInfo, nullptr, &g_vkDevice.framebuffers[i]));
    }
}

//----------------------------------------------------------------------------
// Create frame synchronization objects (double-buffered)
//----------------------------------------------------------------------------
void VK_CreateFrameSyncObjects() {
    vkQueueFamilyIndices_t queueFamilyIndices = VK_FindQueueFamilies(g_vkDevice.physicalDevice);

    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        // Create command pool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

        VK_CHECK(vkCreateCommandPool(g_vkDevice.device, &poolInfo, nullptr, &g_vkDevice.frames[i].commandPool));

        // Allocate command buffer
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g_vkDevice.frames[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(g_vkDevice.device, &allocInfo, &g_vkDevice.frames[i].commandBuffer));

        // Create semaphores
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(g_vkDevice.device, &semaphoreInfo, nullptr, &g_vkDevice.frames[i].imageAvailable));
        VK_CHECK(vkCreateSemaphore(g_vkDevice.device, &semaphoreInfo, nullptr, &g_vkDevice.frames[i].renderFinished));

        // Create fence (signaled initially so first frame doesn't wait)
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(g_vkDevice.device, &fenceInfo, nullptr, &g_vkDevice.frames[i].renderFence));
    }

    // Create dedicated transfer command pool
    VkCommandPoolCreateInfo transferPoolInfo = {};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Short-lived commands

    VK_CHECK(vkCreateCommandPool(g_vkDevice.device, &transferPoolInfo, nullptr, &g_vkDevice.transferCommandPool));

    g_vkDevice.currentFrame = 0;
}

//----------------------------------------------------------------------------
// Destroy device
//----------------------------------------------------------------------------
void VK_DestroyDevice() {
    // Destroy transfer command pool
    if (g_vkDevice.transferCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vkDevice.device, g_vkDevice.transferCommandPool, nullptr);
        g_vkDevice.transferCommandPool = VK_NULL_HANDLE;
    }

    // Destroy per-frame command pools
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (g_vkDevice.frames[i].commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(g_vkDevice.device, g_vkDevice.frames[i].commandPool, nullptr);
            g_vkDevice.frames[i].commandPool = VK_NULL_HANDLE;
        }
    }

    DestroyAllocator();

    if (g_vkDevice.device) {
        vkDestroyDevice(g_vkDevice.device, nullptr);
        g_vkDevice.device = VK_NULL_HANDLE;
        VK_LOG("Logical device destroyed\n");
    }
}

