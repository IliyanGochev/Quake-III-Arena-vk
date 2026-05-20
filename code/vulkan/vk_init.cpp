#define VMA_IMPLEMENTATION
#include "vk_common.h"
#include "vk_driver.h"
#include "vk_state.h"
#include "vk_image.h"
#include "vk_shaders.h"
#include "vk_drawdata.h"

#include "../win32/win_vk.h"

#include <vector>

// Forward declarations for engine string functions (available via qcommon.h but
// not always visible in C++ translation units that include vk_common.h)
// If not available from the engine, provide our own implementations.
extern "C" {
    size_t Q_strlcpy( char* dst, const char* src, size_t size )
    {
        if ( size == 0 ) return 0;
        size_t srcLen = 0;
        while ( src[srcLen] != '\0' ) srcLen++;
        size_t copyLen = ( srcLen < size - 1 ) ? srcLen : size - 1;
        memcpy( dst, src, copyLen );
        dst[copyLen] = '\0';
        return srcLen;
    }

    size_t Q_strlcat( char* dst, const char* src, size_t size )
    {
        size_t dstLen = 0;
        while ( dst[dstLen] != '\0' && dstLen < size ) dstLen++;
        if ( dstLen == 0 ) return dstLen; // empty dst, nothing to concat
        size_t srcLen = 0;
        while ( src[srcLen] != '\0' ) srcLen++;
        size_t available = size - dstLen;
        if ( available == 0 ) return dstLen + srcLen;
        size_t copyLen = ( srcLen < available - 1 ) ? srcLen : available - 1;
        memcpy( dst + dstLen, src, copyLen );
        dst[dstLen + copyLen] = '\0';
        return dstLen + srcLen;
    }
}

// Debug messenger handle
static VkDebugUtilsMessengerEXT g_vkDebugMessenger = VK_NULL_HANDLE;

// Debug utils function pointers (issue #5, #6)
static PFN_vkCmdBeginDebugUtilsLabelEXT  g_vkCmdBeginDebugUtilsLabelEXT = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT    g_vkCmdEndDebugUtilsLabelEXT = nullptr;
static PFN_vkSetDebugUtilsObjectNameEXT  g_vkSetDebugUtilsObjectNameEXT = nullptr;

// Debug object naming helper (issue #5)
void VKDRV_SetDebugObjectName( uint64_t object, VkObjectType type, const char* name )
{
    if ( !g_vkSetDebugUtilsObjectNameEXT || !name )
        return;

    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = object;
    info.pObjectName = name;
    g_vkSetDebugUtilsObjectNameEXT( g_vkDevice, &info );
}

// Debug render phase label helpers (issue #6)
void VKDRV_BeginDebugLabel( VkCommandBuffer cmd, const char* label )
{
    if ( !g_vkCmdBeginDebugUtilsLabelEXT || !label )
        return;

    VkDebugUtilsLabelEXT lbl = {};
    lbl.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    lbl.pLabelName = label;
    lbl.color[0] = 1.0f; lbl.color[1] = 1.0f; lbl.color[2] = 1.0f; lbl.color[3] = 0.0f;
    g_vkCmdBeginDebugUtilsLabelEXT( cmd, &lbl );
}

void VKDRV_EndDebugLabel( VkCommandBuffer cmd )
{
    if ( g_vkCmdEndDebugUtilsLabelEXT )
        g_vkCmdEndDebugUtilsLabelEXT( cmd );
}

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

vkRunState_t  g_vkRunState;
vkDrawState_t g_vkDrawState;

//----------------------------------------------------------------------------
// Instance extension enumeration
//----------------------------------------------------------------------------

static uint32_t instanceExtensionCount = 0;

static const char* const* VKDRV_GetInstanceExtensions( uint32_t* outCount )
{
    static const char* extensions[16];
    uint32_t count = 0;

    // Always required: VK_KHR_surface
    extensions[count++] = VK_KHR_SURFACE_EXTENSION_NAME;

#ifdef _WIN32
    extensions[count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__linux__)
    // Check for XCB first, then Xlib
    extensions[count++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
    extensions[count++] = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#endif

    *outCount = count;
    return extensions;
}

//----------------------------------------------------------------------------
// VKDrv_DriverInit -- called from InitDriver() when r_driver="vulkan"
//----------------------------------------------------------------------------

void VKDrv_DriverInit( void )
{
#ifndef WIN8
    // Assign all GFX_* function pointers
    GFX_Shutdown = VKDrv_Shutdown;
    GFX_UnbindResources = VKDrv_UnbindResources;
    GFX_LastError = VKDrv_LastError;
    GFX_ReadPixels = VKDrv_ReadPixels;
    GFX_ReadDepth = VKDrv_ReadDepth;
    GFX_ReadStencil = VKDrv_ReadStencil;
    GFX_CreateImage = VKDrv_CreateImage;
    GFX_DeleteImage = VKDrv_DeleteImage;
    GFX_UpdateCinematic = VKDrv_UpdateCinematic;
    GFX_DrawImage = VKDrv_DrawImage;
    GFX_GetImageFormat = VKDrv_GetImageFormat;
    GFX_SetGamma = VKDrv_SetGamma;
    GFX_GetFrameImageMemoryUsage = VKDrv_GetFrameImageMemoryUsage;
    GFX_GraphicsInfo = VKDrv_GfxInfo;
    GFX_Clear = VKDrv_Clear;
    GFX_SetProjectionMatrix = VKDrv_SetProjection;
    GFX_GetProjectionMatrix = VKDrv_GetProjection;
    GFX_SetModelViewMatrix = VKDrv_SetModelView;
    GFX_GetModelViewMatrix = VKDrv_GetModelView;
    GFX_SetViewport = VKDrv_SetViewport;
    GFX_Flush = VKDrv_Flush;
    GFX_SetState = VKDrv_SetState;
    GFX_ResetState2D = VKDrv_ResetState2D;
    GFX_ResetState3D = VKDrv_ResetState3D;
    GFX_SetPortalRendering = VKDrv_SetPortalRendering;
    GFX_SetDepthRange = VKDrv_SetDepthRange;
    GFX_SetDrawBuffer = VKDrv_SetDrawBuffer;
    GFX_EndFrame = VKDrv_EndFrame;
    GFX_MakeCurrent = VKDrv_MakeCurrent;
    GFX_ShadowSilhouette = VKDrv_ShadowSilhouette;
    GFX_ShadowFinish = VKDrv_ShadowFinish;
    GFX_DrawSkyBox = VKDrv_DrawSkyBox;
    GFX_DrawBeam = VKDrv_DrawBeam;
    GFX_DrawStageGeneric = VKDrv_DrawStageGeneric;
    GFX_DrawStageVertexLitTexture = VKDrv_DrawStageVertexLitTexture;
    GFX_DrawStageLightmappedMultitexture = VKDrv_DrawStageLightmappedMultitexture;
    GFX_DebugDrawAxis = VKDrv_DebugDrawAxis;
    GFX_DebugDrawTris = VKDrv_DebugDrawTris;
    GFX_DebugDrawNormals = VKDrv_DebugDrawNormals;
    GFX_DebugSetOverdrawMeasureEnabled = VKDrv_DebugSetOverdrawMeasureEnabled;
    GFX_DebugSetTextureMode = VKDrv_DebugSetTextureMode;
    GFX_DebugDrawPolygon = VKDrv_DebugDrawPolygon;
    GFX_BeginTessellate = VKDrv_BeginTessellate;
    GFX_EndTessellate = VKDrv_EndTessellate;
#endif

    VKDRV_Init();
}

void VKDrv_Shutdown( void )
{
    VKDRV_Shutdown();
}

void VKDrv_UnbindResources( void )
{
    // Release GPU references without destroying the device
    VKDRV_DestroyDrawState();
}

size_t VKDrv_LastError( void )
{
    return (size_t)g_vkLastError;
}

//----------------------------------------------------------------------------
// Debug messenger (issue #12)
//----------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData )
{
    (void)types;
    (void)userData;

    const char* severityStr = "UNKNOWN";
    switch ( severity )
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:    severityStr = "ERROR"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:  severityStr = "WARNING"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:     severityStr = "INFO"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:  severityStr = "VERBOSE"; break;
    }

    ri.Printf( PRINT_DEVELOPER, "Vulkan [%s]: %s\n", severityStr, callbackData->pMessage );
    return VK_FALSE;
}

static void VKDRV_CreateDebugMessenger()
{
#ifndef DEBUG
    return;
#endif

    PFN_vkCreateDebugUtilsMessengerEXT pfnCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr( g_vkInstance, "vkCreateDebugUtilsMessengerEXT" ));
    if ( !pfnCreate )
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>( DebugMessengerCallback );

    pfnCreate( g_vkInstance, &createInfo, nullptr, &g_vkDebugMessenger );
}

static void VKDRV_DestroyDebugMessenger()
{
    if ( !g_vkDebugMessenger )
        return;

    PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr( g_vkInstance, "vkDestroyDebugUtilsMessengerEXT" ));
    if ( pfnDestroy )
    {
        pfnDestroy( g_vkInstance, g_vkDebugMessenger, nullptr );
        g_vkDebugMessenger = VK_NULL_HANDLE;
    }
}

//----------------------------------------------------------------------------
// Pipeline cache serialization (issue #13)
//----------------------------------------------------------------------------

// Pipeline cache file format with device/driver version tracking (spec §6.3)
struct VkPipelineCacheHeader {
    uint32_t magic;        // 0x564B4348 ("VKCH")
    uint32_t version;      // cache format version
    uint32_t driverVersion; // driver version at cache creation
    uint32_t deviceID;     // physical device ID
    uint32_t reserved;     // padding to 32-byte alignment
    uint64_t cacheDataSize; // size of Vulkan pipeline cache data following
};

static void VKDRV_LoadPipelineCache()
{
    g_vkPipelineCache = VK_NULL_HANDLE;

    VkPipelineCacheCreateInfo cacheInfo = {};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    // Try to load existing cache file
    char cachePath[MAX_QPATH];
    Q_strlcpy( cachePath, "vk_pipeline_cache.bin", sizeof(cachePath) );

    // Get current device properties for cache validation
    VkPhysicalDeviceProperties curProps = {};
    vkGetPhysicalDeviceProperties( g_vkPhysicalDevice, &curProps );

    int fileSize;
    byte* fileData = nullptr;
    fileSize = ri.FS_ReadFile( cachePath, (void**)&fileData );
    if ( fileData && fileSize > 0 )
    {
        // Validate header
        if ( fileSize >= (int)sizeof(VkPipelineCacheHeader) )
        {
            VkPipelineCacheHeader* header = (VkPipelineCacheHeader*)fileData;
            if ( header->magic != 0x564B4348u )
            {
                ri.Printf( PRINT_DEVELOPER, "Vulkan: Pipeline cache has invalid magic, discarding\n" );
            }
            else if ( header->deviceID != curProps.deviceID ||
                      header->driverVersion != curProps.driverVersion )
            {
                ri.Printf( PRINT_DEVELOPER, "Vulkan: Pipeline cache device/driver mismatch, discarding\n" );
            }
            else
            {
                // Valid cache: use data after the header
                size_t cacheDataSize = fileSize - sizeof(VkPipelineCacheHeader);
                if ( cacheDataSize > 0 && header->cacheDataSize == (uint64_t)cacheDataSize )
                {
                    cacheInfo.initialDataSize = cacheDataSize;
                    cacheInfo.pInitialData = fileData + sizeof(VkPipelineCacheHeader);
                    ri.Printf( PRINT_DEVELOPER, "Vulkan: Loading pipeline cache (%zu bytes)\n", cacheDataSize );
                }
                else
                {
                    ri.Printf( PRINT_DEVELOPER, "Vulkan: Pipeline cache size mismatch, discarding\n" );
                }
            }
        }
        else
        {
            ri.Printf( PRINT_DEVELOPER, "Vulkan: Pipeline cache file too small, discarding\n" );
        }
    }

    if ( vkCreatePipelineCache( g_vkDevice, &cacheInfo, nullptr, &g_vkPipelineCache ) != VK_SUCCESS )
    {
        // Create empty cache if loading failed
        cacheInfo.initialDataSize = 0;
        cacheInfo.pInitialData = nullptr;
        vkCreatePipelineCache( g_vkDevice, &cacheInfo, nullptr, &g_vkPipelineCache );
    }

    if ( fileData )
    {
        ri.Free( fileData );
    }
}

static void VKDRV_SavePipelineCache()
{
    if ( !g_vkPipelineCache )
        return;

    size_t cacheSize = 0;
    vkGetPipelineCacheData( g_vkDevice, g_vkPipelineCache, &cacheSize, nullptr );
    if ( cacheSize == 0 )
        return;

    // Get current device properties for header
    VkPhysicalDeviceProperties curProps = {};
    vkGetPhysicalDeviceProperties( g_vkPhysicalDevice, &curProps );

    // Allocate space for header + cache data
    size_t totalSize = sizeof(VkPipelineCacheHeader) + cacheSize;
    byte* fileData = (byte*)ri.Malloc( totalSize );

    // Write header
    VkPipelineCacheHeader* header = (VkPipelineCacheHeader*)fileData;
    header->magic = 0x564B4348u;
    header->version = 1;
    header->driverVersion = curProps.driverVersion;
    header->deviceID = curProps.deviceID;
    header->reserved = 0;
    header->cacheDataSize = cacheSize;

    // Copy cache data after header
    vkGetPipelineCacheData( g_vkDevice, g_vkPipelineCache, &cacheSize, fileData + sizeof(VkPipelineCacheHeader) );

    ri.FS_WriteFile( "vk_pipeline_cache.bin", (const char*)fileData, totalSize );
    ri.Printf( PRINT_DEVELOPER, "Vulkan: Saved pipeline cache (%zu bytes data)\n", cacheSize );

    ri.Free( fileData );

    vkDestroyPipelineCache( g_vkDevice, g_vkPipelineCache, nullptr );
    g_vkPipelineCache = VK_NULL_HANDLE;
}

//----------------------------------------------------------------------------
// VKDRV_Init -- internal Vulkan initialization
//----------------------------------------------------------------------------

// Vulkan-specific CVARs (issue #14)
static cvar_t* r_vulkanValidation = nullptr;
static cvar_t* r_vulkanVsync = nullptr;
static cvar_t* r_vulkanAnisotropy = nullptr;
static cvar_t* r_vulkanFrameOverlap = nullptr;
static cvar_t* r_vulkanDumpPipelines = nullptr;

void VKDRV_Init()
{
    // Register Vulkan-specific CVARs
    r_vulkanValidation = ri.Cvar_Get( "r_vulkanValidation",
#ifdef DEBUG
        "1",
#else
        "0",
#endif
        CVAR_ARCHIVE );
    r_vulkanVsync = ri.Cvar_Get( "r_vulkanVsync", "1", CVAR_ARCHIVE );
    r_vulkanAnisotropy = ri.Cvar_Get( "r_vulkanAnisotropy", "8", CVAR_ARCHIVE );
    r_vulkanFrameOverlap = ri.Cvar_Get( "r_vulkanFrameOverlap", "2", CVAR_ARCHIVE );
    r_vulkanDumpPipelines = ri.Cvar_Get( "r_vulkanDumpPipelines", "0", CVAR_ARCHIVE );

    // Validate frame overlap cvar against compile-time constant (issue #7)
    if ( r_vulkanFrameOverlap->integer < 1 || r_vulkanFrameOverlap->integer > VK_MAX_FRAMES_IN_FLIGHT )
    {
        ri.Printf( PRINT_WARNING, "Vulkan: r_vulkanFrameOverlap=%d out of range [1,%d], clamping\n",
                    r_vulkanFrameOverlap->integer, VK_MAX_FRAMES_IN_FLIGHT );
        ri.Cvar_Set( "r_vulkanFrameOverlap", va( "%d", VK_MAX_FRAMES_IN_FLIGHT ) );
    }

    // 1. Create Vulkan instance with required extensions
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Quake III Arena";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 32, 0);
    appInfo.pEngineName = "Vulkan Renderer";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    // Gather instance extensions: VK_KHR_surface + platform surface extension
    const char* const* instanceExtensions = VKDRV_GetInstanceExtensions( &instanceExtensionCount );

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = instanceExtensionCount;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

#ifdef DEBUG
    // Enable validation layers (enabled by default in debug builds, controlled by cvar)
    if ( r_vulkanValidation->integer != 0 )
    {
        const char* const validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
        instanceInfo.enabledLayerCount = 1;
        instanceInfo.ppEnabledLayerNames = validationLayers;
    }
    else
    {
        instanceInfo.enabledLayerCount = 0;
        instanceInfo.ppEnabledLayerNames = nullptr;
    }
#else
    instanceInfo.enabledLayerCount = 0;
    instanceInfo.ppEnabledLayerNames = nullptr;
#endif

    VK_CHECK( vkCreateInstance( &instanceInfo, nullptr, &g_vkInstance ) );

    // Create debug messenger (debug builds)
    VKDRV_CreateDebugMessenger();

    // 2. Select physical device (pre-surface: features + extensions only)
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( g_vkInstance, &deviceCount, nullptr );
    ASSERT( deviceCount > 0 );
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices( g_vkInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices )
    {
        if ( IsDeviceSuitable( device ) )
        {
            g_vkPhysicalDevice = device;
            break;
        }
    }
    ASSERT( g_vkPhysicalDevice != VK_NULL_HANDLE );

    // 3. Create window and surface
    VKWnd_Init( (int)vdConfig.vidWidth, (int)vdConfig.vidHeight, r_fullscreen->integer != 0 );
    HWND hwnd = VKWnd_GetWindowHandle();
    if ( !hwnd )
    {
        ri.Error( ERR_FATAL, "Vulkan: Failed to create window\n" );
    }
    g_vkSurface = VKWin_CreateSurface( g_vkInstance, hwnd );
    if ( !g_vkSurface )
    {
        ri.Error( ERR_FATAL, "Vulkan: Failed to create window surface\n" );
    }

    // 4. Post-surface suitability check: verify surface support
    if ( !IsDeviceSuitablePostSurface( g_vkPhysicalDevice ) )
    {
        ri.Error( ERR_FATAL, "Vulkan: Selected device does not support the created surface\n" );
    }

    // 5. Create logical device with required extensions
    uint32_t queueFamilyIndex = FindGraphicsAndPresentQueueFamily( g_vkPhysicalDevice );
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Required device extensions
    static const char* const deviceExtensionsRequired[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME
    };

    // Optional extensions (debug utils for naming/labeling)
    static const char* const deviceExtensionsOptional[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    // Build final extension list with optional extensions if available
    static const char* deviceExtensionList[32];
    uint32_t deviceExtensionCount = 0;

    for ( uint32_t i = 0; i < sizeof(deviceExtensionsRequired) / sizeof(deviceExtensionsRequired[0]); i++ )
    {
        deviceExtensionList[deviceExtensionCount++] = deviceExtensionsRequired[i];
    }

    // Check if debug utils extension is available
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties( g_vkPhysicalDevice, nullptr, &extCount, nullptr );
    if ( extCount > 0 )
    {
        std::vector<VkExtensionProperties> extProps(extCount);
        vkEnumerateDeviceExtensionProperties( g_vkPhysicalDevice, nullptr, &extCount, extProps.data() );

        for ( uint32_t i = 0; i < sizeof(deviceExtensionsOptional) / sizeof(deviceExtensionsOptional[0]); i++ )
        {
            if ( deviceExtensionCount >= 31 )
            {
                ri.Printf( PRINT_WARNING, "Vulkan: Extension list full, skipping optional extensions\n" );
                break;
            }
            for ( uint32_t j = 0; j < extCount; j++ )
            {
                if ( Q_stricmp( deviceExtensionsOptional[i], extProps[j].extensionName ) == 0 )
                {
                    deviceExtensionList[deviceExtensionCount++] = deviceExtensionsOptional[i];
                    break;
                }
            }
        }
    }

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceInfo.enabledExtensionCount = deviceExtensionCount;
     deviceInfo.ppEnabledExtensionNames = deviceExtensionList;
    VkPhysicalDeviceFeatures deviceFeatures = GetRequiredDeviceFeatures();
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    VK_CHECK( vkCreateDevice( g_vkPhysicalDevice, &deviceInfo, nullptr, &g_vkDevice ) );

    vkGetDeviceQueue( g_vkDevice, queueFamilyIndex, 0, &g_vkGraphicsQueue );
    g_vkPresentQueue = g_vkGraphicsQueue;

    // Detect supported depth/stencil format (spec §5.8: D24 primary, D32 fallback)
    VkFormat depthFormats[] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    for ( uint32_t i = 0; i < 2; i++ )
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties( g_vkPhysicalDevice, depthFormats[i], &props );
        if ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT )
        {
            g_vkDepthFormat = depthFormats[i];
            break;
        }
    }
    if ( g_vkDepthFormat == VK_FORMAT_UNDEFINED )
    {
        Com_Error( ERR_FATAL, "Vulkan: no supported depth/stencil format found" );
    }

    // Load debug utils function pointers (issue #5, #6)
    g_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr( g_vkDevice, "vkCmdBeginDebugUtilsLabelEXT" ));
    g_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr( g_vkDevice, "vkCmdEndDebugUtilsLabelEXT" ));
    g_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr( g_vkDevice, "vkSetDebugUtilsObjectNameEXT" ));

    // 6. Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = g_vkPhysicalDevice;
    allocatorInfo.device = g_vkDevice;
    allocatorInfo.instance = g_vkInstance;
    VmaVulkanFunctions vmaFuncs = {};
    vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vmaFuncs;
    VK_CHECK( vmaCreateAllocator( &allocatorInfo, &g_vkAllocator ) );

    // 7. Load pipeline cache from disk
    VKDRV_LoadPipelineCache();

    // 8. Get display mode info and create swapchain
    uint32_t width = (uint32_t)vdConfig.vidWidth;
    uint32_t height = (uint32_t)vdConfig.vidHeight;
    if ( width == 0 ) width = 1024;
    if ( height == 0 ) height = 768;
    VKDRV_CreateSwapchain( width, height );

    // 9. Create depth/stencil target
    VKDRV_CreateDepthTarget( width, height );

    // 10. Create render pass & framebuffers
    VKDRV_CreateRenderPass();
    VKDRV_CreateFramebuffers( g_vkSwapchainFormat, width, height );

    // 11. Create command pool & command buffers
    VKDRV_CreateCommandPool( queueFamilyIndex );

    // 11b. Create dedicated transfer command buffer
    VKDRV_CreateTransferCommandBuffer( queueFamilyIndex );

    // 12. Create synchronization objects
    VKDRV_CreateSyncObjects();

    // 13. Create samplers
    VKDRV_CreateSamplers();

    // 14. Create descriptor system
    VKDRV_CreateDescriptorSystem();

    // Initialize gamma table to identity (will be overwritten by engine's R_InitImages)
    for ( int i = 0; i < 256; i++ )
        g_vkGammaTable[i] = (unsigned char)i;

    // 15. Initialize all draw state
    VKDRV_InitDrawState();

    // 16. Populate descriptor sets with buffer references (after all buffers created)
    VKDRV_PopulateDescriptorSets();
}

void VKDRV_Shutdown()
{
    if ( g_vkDevice )
    {
        vkDeviceWaitIdle( g_vkDevice );

        // Save pipeline cache to disk
        VKDRV_SavePipelineCache();
    }

    VKDRV_DestroyDrawState();
    VKDRV_DestroyDescriptorSystem();
    VKDRV_DestroySamplers();
    VKDRV_DestroySyncObjects();
    VKDRV_DestroyTransferCommandBuffer();
    VKDRV_DestroyCommandPool();
    VKDRV_DestroyFramebuffers();
    VKDRV_DestroyRenderPass();
    VKDRV_DestroyDepthTarget();
    VKDRV_DestroySwapchain();

    if ( g_vkAllocator )
    {
        vmaDestroyAllocator( g_vkAllocator );
        g_vkAllocator = nullptr;
    }

    if ( g_vkDevice )
    {
        vkDestroyDevice( g_vkDevice, nullptr );
        g_vkDevice = VK_NULL_HANDLE;
    }

    VKWin_DestroySurface();
    VKWnd_Shutdown();

    if ( g_vkInstance )
    {
        VKDRV_DestroyDebugMessenger();
        vkDestroyInstance( g_vkInstance, nullptr );
        g_vkInstance = VK_NULL_HANDLE;
    }
}
