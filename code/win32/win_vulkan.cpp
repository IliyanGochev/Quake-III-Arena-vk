// Vulkan window initialization for Win32

#include "../vulkan/vk_common.h"

extern "C" {
#   include "resource.h"
#   include "win_local.h"
}

//=============================================================================
// Globals
//=============================================================================

static HWND g_hWnd = NULL;
static const char* VULKAN_WINDOW_CLASS_NAME = "Quake 3 Vulkan";

//=============================================================================
// Window procedure
//=============================================================================

extern "C" LONG WINAPI MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

//=============================================================================
// Window class registration
//=============================================================================

static qboolean RegisterWindowClass(void)
{
    WNDCLASS wc = {};

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_wv.hInstance;
    wc.hIcon = LoadIcon(g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = VULKAN_WINDOW_CLASS_NAME;

    if (!RegisterClass(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            Com_Printf("ERROR: Failed to register Vulkan window class: %d\n", error);
            return qfalse;
        }
    }

    return qtrue;
}

//=============================================================================
// Window creation
//=============================================================================

static HWND CreateGameWindow(int x, int y, int width, int height, qboolean fullscreen)
{
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    DWORD exStyle = 0;

    if (fullscreen) {
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_TOPMOST;
        x = 0;
        y = 0;
    }

    // Adjust window rect for client area
    RECT rect = { 0, 0, width, height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    HWND hwnd = CreateWindowEx(
        exStyle,
        VULKAN_WINDOW_CLASS_NAME,
        "Quake 3 Arena (Vulkan)",
        style,
        x, y,
        windowWidth, windowHeight,
        NULL,
        NULL,
        g_wv.hInstance,
        NULL
    );

    if (!hwnd) {
        Com_Printf("ERROR: Failed to create Vulkan window: %d\n", GetLastError());
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    return hwnd;
}

//=============================================================================
// Public API
//=============================================================================

extern "C" {

HWND VkWnd_GetHandle(void)
{
    return g_hWnd;
}

qboolean VkWnd_Init(void)
{
    Com_Printf("Initializing Vulkan window subsystem\n");

    if (!RegisterWindowClass()) {
        return qfalse;
    }

    // Get video config
    cvar_t* vid_xpos = ri.Cvar_Get("vid_xpos", "0", 0);
    cvar_t* vid_ypos = ri.Cvar_Get("vid_ypos", "0", 0);
    cvar_t* r_fullscreen = ri.Cvar_Get("r_fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH);

    qboolean fullscreen = (qboolean)(r_fullscreen->integer != 0);

    g_hWnd = CreateGameWindow(
        vid_xpos->integer,
        vid_ypos->integer,
        vdConfig.vidWidth,
        vdConfig.vidHeight,
        fullscreen
    );

    if (!g_hWnd) {
        return qfalse;
    }

    Com_Printf("Vulkan window created: %dx%d\n", vdConfig.vidWidth, vdConfig.vidHeight);
    return qtrue;
}

void VkWnd_Shutdown(void)
{
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        g_hWnd = NULL;
    }

    UnregisterClass(VULKAN_WINDOW_CLASS_NAME, g_wv.hInstance);
}

} // extern "C"
