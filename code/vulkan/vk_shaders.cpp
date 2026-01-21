#include "vk_common.h"
#include "vk_device.h"
#include "vk_shaders.h"
#include "vk_state.h"

//----------------------------------------------------------------------------
// Shader module storage
//----------------------------------------------------------------------------
struct vkShaderModules_t {
    // [shader type][stage] - 0 = vertex, 1 = fragment
    VkShaderModule modules[VK_SHADER_COUNT][2];
};

static vkShaderModules_t g_vkShaderModules;

//----------------------------------------------------------------------------
// Load single shader module from SPIR-V file
//----------------------------------------------------------------------------
VkShaderModule VK_LoadShaderModule(const char* filename) {
    // Construct full path
    char fullPath[256];
    Com_sprintf(fullPath, sizeof(fullPath), "vulkan/shaders/compiled/%s", filename);

    // Load file
    void* spirvData = nullptr;
    int length = ri.FS_ReadFile(fullPath, &spirvData);

    if (length <= 0 || !spirvData) {
        ri.Printf(PRINT_WARNING, "WARNING: Failed to load shader: %s\n", fullPath);
        return VK_NULL_HANDLE;
    }

    // Validate SPIR-V magic number
    if (length < 4 || *((uint32_t*)spirvData) != 0x07230203) {
        ri.Printf(PRINT_WARNING, "WARNING: Invalid SPIR-V file: %s\n", fullPath);
        ri.FS_FreeFile(spirvData);
        return VK_NULL_HANDLE;
    }

    // Create shader module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = length;
    createInfo.pCode = (uint32_t*)spirvData;

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(g_vkDevice.device, &createInfo, nullptr, &shaderModule);

    ri.FS_FreeFile(spirvData);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "WARNING: Failed to create shader module for %s: 0x%08X\n", fullPath, result);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

//----------------------------------------------------------------------------
// Load all shader modules
//----------------------------------------------------------------------------
void VK_LoadAllShaders() {
    ri.Printf(PRINT_ALL, "Loading Vulkan shaders...\n");

    // Initialize to null
    memset(&g_vkShaderModules, 0, sizeof(g_vkShaderModules));

    // Load single-texture shaders
    g_vkShaderModules.modules[VK_SHADER_SINGLE_TEXTURE][0] = VK_LoadShaderModule("genericst_vs.spv");
    g_vkShaderModules.modules[VK_SHADER_SINGLE_TEXTURE][1] = VK_LoadShaderModule("genericst_ps.spv");

    // Load multi-texture shaders
    g_vkShaderModules.modules[VK_SHADER_MULTI_TEXTURE][0] = VK_LoadShaderModule("genericmt_vs.spv");
    g_vkShaderModules.modules[VK_SHADER_MULTI_TEXTURE][1] = VK_LoadShaderModule("genericmt_ps.spv");

    // Load skybox shaders
    g_vkShaderModules.modules[VK_SHADER_SKYBOX][0] = VK_LoadShaderModule("skybox_vs.spv");
    g_vkShaderModules.modules[VK_SHADER_SKYBOX][1] = VK_LoadShaderModule("skybox_ps.spv");

    // Load fullscreen quad / 2D shaders
    g_vkShaderModules.modules[VK_SHADER_FSQ][0] = VK_LoadShaderModule("fsq_vs.spv");
    g_vkShaderModules.modules[VK_SHADER_FSQ][1] = VK_LoadShaderModule("fsq_ps.spv");

    // Verify all shaders loaded successfully
    int loadedCount = 0;
    for (int i = 0; i < VK_SHADER_COUNT; i++) {
        if (g_vkShaderModules.modules[i][0] != VK_NULL_HANDLE &&
            g_vkShaderModules.modules[i][1] != VK_NULL_HANDLE) {
            loadedCount++;
        } else {
            ri.Printf(PRINT_WARNING, "WARNING: Shader type %d failed to load completely\n", i);
        }
    }

    ri.Printf(PRINT_ALL, "...loaded %d/%d shader types\n", loadedCount, VK_SHADER_COUNT);

    if (loadedCount != VK_SHADER_COUNT) {
        ri.Error(ERR_FATAL, "Failed to load all required shaders. Please run compile_shaders.bat in code/vulkan/shaders/\n");
    }
}

//----------------------------------------------------------------------------
// Destroy all shader modules
//----------------------------------------------------------------------------
void VK_DestroyAllShaders() {
    ri.Printf(PRINT_ALL, "Destroying Vulkan shaders...\n");

    for (int i = 0; i < VK_SHADER_COUNT; i++) {
        for (int j = 0; j < 2; j++) {
            if (g_vkShaderModules.modules[i][j] != VK_NULL_HANDLE) {
                vkDestroyShaderModule(g_vkDevice.device, g_vkShaderModules.modules[i][j], nullptr);
                g_vkShaderModules.modules[i][j] = VK_NULL_HANDLE;
            }
        }
    }
}

//----------------------------------------------------------------------------
// Get shader module for shader type and stage
//----------------------------------------------------------------------------
VkShaderModule VK_GetShaderModule(vkShaderType_t shaderType, VkShaderStageFlagBits stage) {
    if (shaderType < 0 || shaderType >= VK_SHADER_COUNT) {
        ri.Printf(PRINT_WARNING, "WARNING: Invalid shader type %d\n", shaderType);
        return VK_NULL_HANDLE;
    }

    int stageIndex = (stage == VK_SHADER_STAGE_VERTEX_BIT) ? 0 : 1;
    return g_vkShaderModules.modules[shaderType][stageIndex];
}

//----------------------------------------------------------------------------
// Create shader stage info
//----------------------------------------------------------------------------
VkPipelineShaderStageCreateInfo VK_CreateShaderStage(VkShaderStageFlagBits stage,
                                                      VkShaderModule module,
                                                      const char* entryPoint) {
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage;
    stageInfo.module = module;
    stageInfo.pName = entryPoint;  // Typically "main"
    return stageInfo;
}
