#include "win_vk.h"
#include <vulkan/vulkan_win32.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern VkInstance g_vkInstance;

static VkSurfaceKHR g_vkSurface = VK_NULL_HANDLE;
static HWND         g_vkHWND    = nullptr;

#define VK_WINDOW_CLASS_NAME  "Quake 3: Arena (Vulkan)"

//----------------------------------------------------------------------------
// Window procedure
//----------------------------------------------------------------------------
static LONG WINAPI VulkanWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if ( uMsg == WM_DESTROY && hWnd == g_vkHWND )
    {
        g_vkHWND = nullptr;
    }

    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

//----------------------------------------------------------------------------
// Register the window class
//----------------------------------------------------------------------------
static BOOL RegisterWindowClass()
{
    WNDCLASS wc = {};

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = (WNDPROC)VulkanWndProc;
    wc.hInstance     = GetModuleHandle( nullptr );
    wc.hCursor       = LoadCursor( nullptr, IDC_ARROW );
    wc.lpszClassName = VK_WINDOW_CLASS_NAME;

    return ::RegisterClass( &wc ) != 0;
}

//----------------------------------------------------------------------------
// Create the game window
//----------------------------------------------------------------------------
static HWND CreateGameWindow( int x, int y, int width, int height, BOOL fullscreen )
{
    UINT exStyle;
    UINT style;

    if ( fullscreen )
    {
        exStyle = WS_EX_TOPMOST;
        style   = WS_POPUP | WS_VISIBLE | WS_SYSMENU;
    }
    else
    {
        exStyle = 0;
        style   = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
    }

    RECT rect = { x, y, x + width, y + height };
    AdjustWindowRectEx( &rect, style, FALSE, exStyle );

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

    return CreateWindowEx(
        exStyle,
        VK_WINDOW_CLASS_NAME,
        "Quake 3: Arena",
        style,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle( nullptr ),
        nullptr );
}

//----------------------------------------------------------------------------
// Public API -- mirrors the D3D window functions so the Vulkan driver
// gets a proper HWND without pulling in D3D11 headers or state.
//----------------------------------------------------------------------------
void VKWnd_Init( int width, int height, BOOL fullscreen )
{
    if ( !RegisterWindowClass() )
    {
        return;
    }

    g_vkHWND = CreateGameWindow( 0, 0, width, height, fullscreen );

    if ( g_vkHWND )
    {
        ::ShowWindow( g_vkHWND, SW_SHOW );
        ::UpdateWindow( g_vkHWND );
        ::SetForegroundWindow( g_vkHWND );
        ::SetFocus( g_vkHWND );
    }
}

void VKWnd_Shutdown( void )
{
    if ( g_vkHWND )
    {
        ::DestroyWindow( g_vkHWND );
        g_vkHWND = nullptr;
    }

    ::UnregisterClass( VK_WINDOW_CLASS_NAME, GetModuleHandle( nullptr ) );
}

HWND VKWnd_GetWindowHandle( void )
{
    return g_vkHWND;
}

//----------------------------------------------------------------------------
// Vulkan surface creation / destruction
//----------------------------------------------------------------------------
VkSurfaceKHR VKWin_CreateSurface( VkInstance instance, HWND hwnd )
{
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance  = GetModuleHandle( nullptr );
    createInfo.hwnd       = hwnd;

    if ( vkCreateWin32SurfaceKHR( instance, &createInfo, nullptr, &g_vkSurface ) != VK_SUCCESS )
    {
        return VK_NULL_HANDLE;
    }

    return g_vkSurface;
}

void VKWin_DestroySurface()
{
    if ( g_vkSurface != VK_NULL_HANDLE )
    {
        vkDestroySurfaceKHR( g_vkInstance, g_vkSurface, nullptr );
        g_vkSurface = VK_NULL_HANDLE;
    }
}
