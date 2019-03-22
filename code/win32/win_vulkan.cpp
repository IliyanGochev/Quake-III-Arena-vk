#include "../vulkan/vk_common.h"
#include "../vulkan/vk_device.h"

extern "C" {
#	include "resource.h"
#	include "win_local.h"
}

#define WINDOW_CLASS_NAME "Quake 3: Arena (Vulkan)"

HWND g_vkhWnd = nullptr;

static LONG WINAPI VulkanWndProc(
	HWND	hWnd,
	UINT	uMsg,
	WPARAM  wParam,
	LPARAM  lParam
)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		// Handle window closing
		if (hWnd == g_vkhWnd)
		{
			g_vkhWnd = nullptr;
		}
		break;
	case WM_SIZE:		
		break;	
	default:
		break;
	}
	return  MainWndProc(hWnd, uMsg, wParam, lParam);
}

static BOOL RegisterWindowClassVK()
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= (WNDPROC) VulkanWndProc;
	wc.cbClsExtra	= 0;
	wc.hInstance	= g_wv.hInstance;
	wc.hIcon		= LoadIconA(g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName= WINDOW_CLASS_NAME; // TODO: rename?

	return ::RegisterClass(&wc) != 0;
}

static HWND CreateGameWindowVK(int x, int y, int vid_width, int vid_height, bool fullscreen)
{
	HWND hWnd;
	UINT style;
	UINT exStyle;
	
	if (fullscreen)
	{
		exStyle = WS_EX_TOPMOST;
		style	= WS_POPUP | WS_VISIBLE | WS_SYSMENU;
	}
	else
	{
		exStyle = 0;
		style	= WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
	}

	RECT rect = { 
		x, 
		y, 
		x + vid_width, 
		y + vid_height };

	AdjustWindowRectEx(&rect, style, FALSE, exStyle);

	// Position on screen
	// TODO: Find main screen
	if (rect.top < 0 )
	{
		rect.bottom -= rect.top;
		rect.top = 0;
	}	
	if (rect.left < 0 )
	{
		rect.right -= rect.left;
		rect.left = 0;
	}

	hWnd = CreateWindowEx(
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
		NULL);
	return hWnd;
}
VK_PUBLIC void VKWnd_Init( void )
{
	ri.Printf(PRINT_ALL, "Initializing Vulkan subsystem\n");

	if (!RegisterWindowClassVK() )
	{
		ri.Error(ERR_FATAL, "Failed registering Vulkan Window Class!");
		exit(-1);
	}

	auto fullscreen = r_fullscreen->integer != 0;
	cvar_t* vid_xpos = ri.Cvar_Get("vid_xpos", "", 0);
	cvar_t* vid_ypos = ri.Cvar_Get("vid_ypos", "", 0);
	cvar_t* r_fullscreen = Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);

	g_vkhWnd = CreateGameWindowVK(
		vid_xpos->integer,
		vid_ypos->integer,
		vdConfig.vidWidth,
		vdConfig.vidHeight,
		fullscreen
	);

	if(!g_vkhWnd)
	{
		ri.Error(ERR_FATAL, "Failed to create Vulkan Window\n");
		return;
	}
		
	VkCreateInstance();

	::ShowWindow(g_vkhWnd, SW_SHOW);
	::UpdateWindow(g_vkhWnd);
	::SetForegroundWindow(g_vkhWnd);
	::SetFocus(g_vkhWnd);
}

VK_PUBLIC void VKWnd_Shutdown( void)
{	
	VkDestroyInstance();
}

VK_PUBLIC HWND VKWnd_GetWindowHandle( void) {
	return g_vkhWnd;
}

VK_PUBLIC HINSTANCE VKWnd_GetInstance(void) {
	return g_wv.hInstance;
}