#pragma once

#include "vk_common.h"
#include "vk_state.h"

//----------------------------------------------------------------------------
// Shader management functions
//----------------------------------------------------------------------------

// Load all shader modules
void VK_LoadAllShaders();

// Destroy all shader modules
void VK_DestroyAllShaders();

// Load single shader module from SPIR-V file
VkShaderModule VK_LoadShaderModule(const char* filename);

// Create shader stage info
VkPipelineShaderStageCreateInfo VK_CreateShaderStage(VkShaderStageFlagBits stage,
                                                      VkShaderModule module,
                                                      const char* entryPoint);

// Get shader module for shader type and stage
VkShaderModule VK_GetShaderModule(vkShaderType_t shaderType, VkShaderStageFlagBits stage);

