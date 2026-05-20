#pragma once

// @pjb: SPIR-V shader loading functions

void InitShaders();
void DestroyShaders();

// Load a SPIR-V shader module from a compiled .spv file
VkShaderModule LoadShaderModule( const char* name );

// Create a vertex stage info from a shader module
VkPipelineShaderStageCreateInfo CreateVertexShaderStage( VkShaderModule module );

// Create a fragment stage info from a shader module
VkPipelineShaderStageCreateInfo CreateFragmentShaderStage( VkShaderModule module );
