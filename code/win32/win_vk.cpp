#include "../vulkan/vk_common.h"
#include "../vulkan/vk_device.h"
#include "../vulkan/vk_image.h"
#include "win_vk.h"

extern "C" {
#   include "resource.h"
#   include "win_local.h"
}

#define	WINDOW_CLASS_NAME	"Quake 3: Arena (Vulkan)"

extern HWND g_hWnd;

//----------------------------------------------------------------------------
// WndProc: Intercepts window events before passing them on to the game.
//----------------------------------------------------------------------------
static LONG WINAPI VulkanWndProc(
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
        // If our window was closed, lose our cached handle
		if (hWnd == g_hWnd) {
            g_hWnd = NULL;
        }

        // We want to pass this message on to the MainWndProc
        break;
	case WM_SIZE:

        // @pjb: todo: recreate swapchain?
        // @pjb: actually fuck that. disable window sizing.

        break;

    default:
        break;
    }

    return MainWndProc( hWnd, uMsg, wParam, lParam );
}

//----------------------------------------------------------------------------
// Register the window class.
//----------------------------------------------------------------------------
static BOOL RegisterWindowClass()
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC) VulkanWndProc;
    wc.cbClsExtra = 0;
    wc.hInstance = g_wv.hInstance;
    wc.hIcon = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1) );
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    return ::RegisterClass( &wc ) != 0;
}

//----------------------------------------------------------------------------
// Creates a window to render our game in.
//----------------------------------------------------------------------------
static HWND CreateGameWindow( int x, int y, int width, int height, bool fullscreen )
{
    UINT exStyle;
    UINT style;

	if ( fullscreen )
	{
		exStyle = WS_EX_TOPMOST;
		style = WS_POPUP|WS_VISIBLE|WS_SYSMENU;
	}
	else
	{
		exStyle = 0;
		style = WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE|WS_SYSMENU;
	}

    RECT rect = { x, y, x + width, y + height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    // Make sure it's on-screen
    if ( rect.top < 0 )
    {
        rect.bottom -= rect.top;
        rect.top = 0;
    }
    if ( rect.left < 0 )
    {
        rect.right -= rect.left;
        rect.left = 0;
    }

    // @pjb: todo: right and bottom edges of the monitor

    return CreateWindowEx(
        exStyle,
        WINDOW_CLASS_NAME,
        "Quake 3: Arena",
        style,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        g_wv.hInstance,
        NULL );
}

//----------------------------------------------------------------------------
// Create VkSurfaceKHR for Win32
//----------------------------------------------------------------------------
static void CreateVulkanSurface()
{
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = g_wv.hInstance;
    createInfo.hwnd = g_hWnd;

    VK_CHECK(vkCreateWin32SurfaceKHR(g_vkDevice.instance, &createInfo, nullptr, &g_vkDevice.surface));

    ri.Printf(PRINT_ALL, "...created Vulkan Win32 surface\n");
}

//----------------------------------------------------------------------------
// Creates a window, initializes Vulkan device and sets up rendering state.
//----------------------------------------------------------------------------
void VKWnd_Init( void )
{
	ri.Printf( PRINT_ALL, "Initializing Vulkan subsystem\n" );

    if ( !RegisterWindowClass() )
    {
        ri.Error( ERR_FATAL, "Failed to register Vulkan window class.\n" );
        return;
    }

    // Get configuration
    cvar_t* vid_xpos = ri.Cvar_Get ("vid_xpos", "", 0);
    cvar_t* vid_ypos = ri.Cvar_Get ("vid_ypos", "", 0);
    cvar_t* r_fullscreen = ri.Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );

    bool fullscreen = r_fullscreen->integer != 0;

    // Create window
    g_hWnd = CreateGameWindow(
        vid_xpos->integer,
        vid_ypos->integer,
        vdConfig.vidWidth,
        vdConfig.vidHeight,
        fullscreen);
    if ( !g_hWnd )
    {
        ri.Error( ERR_FATAL, "Failed to create Vulkan window.\n" );
        return;
    }

    ri.Printf(PRINT_ALL, "...created game window\n");

	// Initialize Vulkan instance
    VK_CreateInstance();

    // Create surface (needed before physical device selection)
    CreateVulkanSurface();

    // Pick physical device (needs surface for present queue)
    VK_PickPhysicalDevice();

    // Create logical device and get queues
    VK_CreateLogicalDevice();

    // Initialize VMA allocator
    VK_CreateAllocator();

    ri.Printf(PRINT_ALL, "...Vulkan device initialized\n");

    // Create swapchain (Phase 1.4)
    VK_CreateSwapchain();

    // Create render pass (Phase 1.5)
    VK_CreateRenderPass();

    // Create framebuffers (Phase 1.6)
    VK_CreateFramebuffers();

    // Create frame synchronization objects (Phase 1.7)
    VK_CreateFrameSyncObjects();

    ri.Printf(PRINT_ALL, "...Phase 1 (Foundation) complete!\n");

    // Show window
    ::ShowWindow( g_hWnd, SW_SHOW );
    ::UpdateWindow( g_hWnd );
	::SetForegroundWindow( g_hWnd );
	::SetFocus( g_hWnd );

    ri.Printf(PRINT_ALL, "...Vulkan initialization complete\n");
}

//----------------------------------------------------------------------------
// Cleans up and stops Vulkan, and closes the window.
//----------------------------------------------------------------------------
void VKWnd_Shutdown( void )
{
    ri.Printf(PRINT_ALL, "Shutting down Vulkan subsystem\n");

    // Wait for device to finish all operations
    if (g_vkDevice.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_vkDevice.device);
    }

    // Destroy frame sync objects (semaphores, fences, command pools)
    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (g_vkDevice.frames[i].renderFence != VK_NULL_HANDLE) {
            vkDestroyFence(g_vkDevice.device, g_vkDevice.frames[i].renderFence, nullptr);
            g_vkDevice.frames[i].renderFence = VK_NULL_HANDLE;
        }
        if (g_vkDevice.frames[i].imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_vkDevice.device, g_vkDevice.frames[i].imageAvailable, nullptr);
            g_vkDevice.frames[i].imageAvailable = VK_NULL_HANDLE;
        }
        if (g_vkDevice.frames[i].renderFinished != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_vkDevice.device, g_vkDevice.frames[i].renderFinished, nullptr);
            g_vkDevice.frames[i].renderFinished = VK_NULL_HANDLE;
        }
        if (g_vkDevice.frames[i].commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(g_vkDevice.device, g_vkDevice.frames[i].commandPool, nullptr);
            g_vkDevice.frames[i].commandPool = VK_NULL_HANDLE;
        }
    }

    // Destroy framebuffers
    for (size_t i = 0; i < g_vkDevice.framebuffers.size(); i++) {
        if (g_vkDevice.framebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(g_vkDevice.device, g_vkDevice.framebuffers[i], nullptr);
        }
    }
    g_vkDevice.framebuffers.clear();

    // Destroy render pass
    if (g_vkDevice.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vkDevice.device, g_vkDevice.renderPass, nullptr);
        g_vkDevice.renderPass = VK_NULL_HANDLE;
    }

    // Destroy MSAA color image if it exists
    if (g_vkDevice.msaaColorImage != VK_NULL_HANDLE) {
        if (g_vkDevice.msaaColorView != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vkDevice.device, g_vkDevice.msaaColorView, nullptr);
            g_vkDevice.msaaColorView = VK_NULL_HANDLE;
        }
        vkDestroyImage(g_vkDevice.device, g_vkDevice.msaaColorImage, nullptr);
        vmaFreeMemory(g_vkDevice.allocator, g_vkDevice.msaaColorAllocation);
        g_vkDevice.msaaColorImage = VK_NULL_HANDLE;
    }

    // Destroy depth image
    if (g_vkDevice.depthImage != VK_NULL_HANDLE) {
        if (g_vkDevice.depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vkDevice.device, g_vkDevice.depthImageView, nullptr);
            g_vkDevice.depthImageView = VK_NULL_HANDLE;
        }
        vkDestroyImage(g_vkDevice.device, g_vkDevice.depthImage, nullptr);
        vmaFreeMemory(g_vkDevice.allocator, g_vkDevice.depthAllocation);
        g_vkDevice.depthImage = VK_NULL_HANDLE;
    }

    // Destroy swapchain image views
    for (size_t i = 0; i < g_vkDevice.swapchainImageViews.size(); i++) {
        if (g_vkDevice.swapchainImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vkDevice.device, g_vkDevice.swapchainImageViews[i], nullptr);
        }
    }
    g_vkDevice.swapchainImageViews.clear();

    // Destroy swapchain
    if (g_vkDevice.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vkDevice.device, g_vkDevice.swapchain, nullptr);
        g_vkDevice.swapchain = VK_NULL_HANDLE;
    }

    // Destroy all texture images
    VK_DestroyAllImages();

    // Destroy allocator
    if (g_vkDevice.allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(g_vkDevice.allocator);
        g_vkDevice.allocator = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (g_vkDevice.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vkDevice.instance, g_vkDevice.surface, nullptr);
        g_vkDevice.surface = VK_NULL_HANDLE;
    }

    // Destroy logical device
    if (g_vkDevice.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_vkDevice.device, nullptr);
        g_vkDevice.device = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    #ifdef _DEBUG
    if (g_vkDevice.debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            g_vkDevice.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(g_vkDevice.instance, g_vkDevice.debugMessenger, nullptr);
        }
        g_vkDevice.debugMessenger = VK_NULL_HANDLE;
    }
    #endif

    // Destroy instance
    if (g_vkDevice.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_vkDevice.instance, nullptr);
        g_vkDevice.instance = VK_NULL_HANDLE;
    }

    // Destroy window
    ::UnregisterClass( WINDOW_CLASS_NAME, g_wv.hInstance );
    ::DestroyWindow( g_hWnd );
    g_hWnd = NULL;

    ri.Printf(PRINT_ALL, "...Vulkan shutdown complete\n");
}

//----------------------------------------------------------------------------
// Returns the window handle
//----------------------------------------------------------------------------
HWND VKWnd_GetWindowHandle( void )
{
    return g_hWnd;
}

//----------------------------------------------------------------------------
// Get window dimensions
//----------------------------------------------------------------------------
void VKWnd_GetWindowSize(int* width, int* height)
{
    if (g_hWnd && width && height) {
        RECT rect;
        GetClientRect(g_hWnd, &rect);
        *width = rect.right - rect.left;
        *height = rect.bottom - rect.top;
    }
}
