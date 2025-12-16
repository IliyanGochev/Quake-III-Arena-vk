// @pjb: Vulkan rendering backend - shader management
#ifndef VK_SHADERS_H
#define VK_SHADERS_H

#include "vk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Shader types
//=============================================================================

typedef enum {
    VK_SHADER_GENERIC_ST,       // Single-texture generic
    VK_SHADER_GENERIC_MT,       // Multi-texture generic
    VK_SHADER_SKYBOX,           // Skybox
    VK_SHADER_IMAGE2D,          // 2D image rendering
    VK_SHADER_FOG,              // Fog pass
    VK_SHADER_SHADOW,           // Shadow volume
    VK_SHADER_COUNT
} vkShaderType_t;

//=============================================================================
// Functions
//=============================================================================

void VkShaders_Init(void);
void VkShaders_Shutdown(void);

// Get shader modules
VkShaderModule VkShaders_GetVertexShader(vkShaderType_t type);
VkShaderModule VkShaders_GetFragmentShader(vkShaderType_t type);

// Load a shader module from SPIRV file
VkShaderModule VkShaders_LoadModule(const char* filename);

#ifdef __cplusplus
}
#endif

#endif // VK_SHADERS_H
