#ifndef __WIN_VK_H__
#define __WIN_VK_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Window management -- Vulkan-specific, no D3D dependency */
void   VKWnd_Init( int width, int height, BOOL fullscreen );
void   VKWnd_Shutdown( void );
HWND   VKWnd_GetWindowHandle( void );

/* Vulkan surface */
VkSurfaceKHR VKWin_CreateSurface( VkInstance instance, HWND hwnd );
void         VKWin_DestroySurface( void );

#ifdef __cplusplus
}
#endif

#endif
