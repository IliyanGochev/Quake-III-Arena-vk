#ifndef __WIN_VULKAN_H__
#define __WIN_VULKAN_H__

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	void VKWnd_Init(void);
	void VKWnd_Shutdown(void);
	HWND VKWnd_GetWindowHandle(void);
#ifdef __cplusplus
}
#endif

#endif