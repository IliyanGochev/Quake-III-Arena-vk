// @pjb: Vulkan rendering backend - shader management implementation

#include "vk_shaders.h"
#include <string.h>

//=============================================================================
// Shader module storage
//=============================================================================

typedef struct vkShaderPair_s {
    VkShaderModule vertex;
    VkShaderModule fragment;
} vkShaderPair_t;

static vkShaderPair_t s_shaders[VK_SHADER_COUNT];

//=============================================================================
// Shader file paths
//=============================================================================

static const char* s_shaderPaths[VK_SHADER_COUNT][2] = {
    // VK_SHADER_GENERIC_ST
    { "spirv/genericst_vs.spirv", "spirv/genericst_ps.spirv" },
    // VK_SHADER_GENERIC_MT
    { "spirv/genericmt_vs.spirv", "spirv/genericmt_ps.spirv" },
    // VK_SHADER_SKYBOX
    { "spirv/skybox_vs.spirv", "spirv/skybox_ps.spirv" },
    // VK_SHADER_IMAGE2D
    { "spirv/image2d_vs.spirv", "spirv/image2d_ps.spirv" },
    // VK_SHADER_FOG (use generic for now)
    { "spirv/genericst_vs.spirv", "spirv/genericst_ps.spirv" },
    // VK_SHADER_SHADOW (use generic for now)
    { "spirv/genericst_vs.spirv", "spirv/genericst_ps.spirv" },
};

//=============================================================================
// Shader loading
//=============================================================================

VkShaderModule VkShaders_LoadModule(const char* filename)
{
    void* data;
    int length;

    length = ri.FS_ReadFile(filename, &data);
    if (length <= 0 || !data) {
        Com_Printf("WARNING: Failed to load shader %s\n", filename);
        return VK_NULL_HANDLE;
    }

    // SPIRV must be 4-byte aligned
    if (length % 4 != 0) {
        Com_Printf("WARNING: Shader %s has invalid size (not 4-byte aligned)\n", filename);
        ri.FS_FreeFile(data);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = length;
    createInfo.pCode = (const uint32_t*)data;

    VkShaderModule module;
    VkResult result = qvkCreateShaderModule(vk.device, &createInfo, NULL, &module);

    ri.FS_FreeFile(data);

    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateShaderModule failed for %s: %s\n", filename, Vk_ResultString(result));
        return VK_NULL_HANDLE;
    }

    Com_Printf("Loaded shader: %s\n", filename);
    return module;
}

//=============================================================================
// Initialization
//=============================================================================

void VkShaders_Init(void)
{
    Com_Memset(s_shaders, 0, sizeof(s_shaders));

    Com_Printf("Loading Vulkan shaders...\n");

    for (int i = 0; i < VK_SHADER_COUNT; i++) {
        s_shaders[i].vertex = VkShaders_LoadModule(s_shaderPaths[i][0]);
        s_shaders[i].fragment = VkShaders_LoadModule(s_shaderPaths[i][1]);

        if (!s_shaders[i].vertex || !s_shaders[i].fragment) {
            Com_Printf("WARNING: Shader pair %d incomplete\n", i);
        }
    }
}

void VkShaders_Shutdown(void)
{
    for (int i = 0; i < VK_SHADER_COUNT; i++) {
        if (s_shaders[i].vertex) {
            qvkDestroyShaderModule(vk.device, s_shaders[i].vertex, NULL);
        }
        if (s_shaders[i].fragment) {
            qvkDestroyShaderModule(vk.device, s_shaders[i].fragment, NULL);
        }
    }

    Com_Memset(s_shaders, 0, sizeof(s_shaders));
}

//=============================================================================
// Getters
//=============================================================================

VkShaderModule VkShaders_GetVertexShader(vkShaderType_t type)
{
    if (type < 0 || type >= VK_SHADER_COUNT) {
        return VK_NULL_HANDLE;
    }
    return s_shaders[type].vertex;
}

VkShaderModule VkShaders_GetFragmentShader(vkShaderType_t type)
{
    if (type < 0 || type >= VK_SHADER_COUNT) {
        return VK_NULL_HANDLE;
    }
    return s_shaders[type].fragment;
}
