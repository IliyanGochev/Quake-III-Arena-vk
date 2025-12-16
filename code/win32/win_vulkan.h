// Vulkan window initialization for Win32
#ifndef WIN_VULKAN_H
#define WIN_VULKAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include "../game/q_shared.h"

HWND VkWnd_GetHandle(void);
qboolean VkWnd_Init(void);
void VkWnd_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // WIN_VULKAN_H
