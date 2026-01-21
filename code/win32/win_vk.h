#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------
// Platform-specific Vulkan functions
//----------------------------------------------------------------------------

// Initialize Vulkan window and surface
void VKWnd_Init(void);

// Shutdown Vulkan window
void VKWnd_Shutdown(void);

// Get window handle
HWND VKWnd_GetWindowHandle(void);

// Get window dimensions
void VKWnd_GetWindowSize(int* width, int* height);

#ifdef __cplusplus
}
#endif

