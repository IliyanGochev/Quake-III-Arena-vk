#include "vk_common.h"
#include "vk_device.h"
#include "vk_state.h"
#include "vk_shaders.h"
#include <unordered_map>

//----------------------------------------------------------------------------
// Global state
//----------------------------------------------------------------------------
vkPipelineState_t g_vkPipelines;
vkRunState_t g_vkRunState;

//----------------------------------------------------------------------------
// Create descriptor set layouts
//----------------------------------------------------------------------------
void VK_CreateDescriptorSetLayouts() {
    // Set 0: View (per-frame/per-portal)
    // Binding 0: ViewVS uniform buffer
    // Binding 1: ViewPS uniform buffer
    {
        VkDescriptorSetLayoutBinding bindings[2] = {};

        // Binding 0: ViewVS uniform buffer (vertex shader)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1: ViewPS uniform buffer (fragment shader)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(g_vkDevice.device, &layoutInfo, nullptr, &g_vkPipelines.setLayouts[0]));
    }

    // Set 1: Material (per-stage)
    // Binding 0: Stage data uniform buffer
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        VK_CHECK(vkCreateDescriptorSetLayout(g_vkDevice.device, &layoutInfo, nullptr, &g_vkPipelines.setLayouts[1]));
    }

    // Set 2: Textures (per-texture)
    // Binding 0: Diffuse texture sampler
    // Binding 1: Lightmap texture sampler (optional for multi-texture)
    {
        VkDescriptorSetLayoutBinding bindings[2] = {};

        // Binding 0: Diffuse texture
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1: Lightmap texture
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(g_vkDevice.device, &layoutInfo, nullptr, &g_vkPipelines.setLayouts[2]));
    }
}

//----------------------------------------------------------------------------
// Create descriptor pool
//----------------------------------------------------------------------------
void VK_CreateDescriptorPool() {
    // Pool sizes
    VkDescriptorPoolSize poolSizes[2] = {};

    // Uniform buffers (Set 0 has 2, Set 1 has 1)
    // Estimate: 3 sets for Set 0 (frame 0, frame 1, portal), 1024 for Set 1 (stages)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3 * 2 + 1024 * 1;  // 1030 uniform buffers

    // Combined image samplers (Set 2 has 2 bindings)
    // Estimate: 4096 texture descriptor sets (2x MAX_DRAWIMAGES)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 4096 * 2;  // 8192 image samplers

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 3 + 1024 + 4096;  // Total descriptor sets
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(g_vkDevice.device, &poolInfo, nullptr, &g_vkPipelines.descriptorPool));
}

//----------------------------------------------------------------------------
// Create pipeline layout
//----------------------------------------------------------------------------
void VK_CreatePipelineLayout() {
    // Push constant range (for skybox eye position)
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 4;  // vec4 for eye position

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pSetLayouts = g_vkPipelines.setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(g_vkDevice.device, &layoutInfo, nullptr, &g_vkPipelines.pipelineLayout));
}

//----------------------------------------------------------------------------
// Initialize pipeline state
//----------------------------------------------------------------------------
void VK_InitPipelineState() {
    // Initialize pipeline state (DO NOT use memset on structures with STL containers!)
    g_vkPipelines.pipelineLayout = VK_NULL_HANDLE;
    g_vkPipelines.descriptorPool = VK_NULL_HANDLE;
    for (int i = 0; i < 3; i++) {
        g_vkPipelines.setLayouts[i] = VK_NULL_HANDLE;
    }
    for (int i = 0; i < VK_SHADER_COUNT * 2; i++) {
        g_vkPipelines.shaderModules[i] = VK_NULL_HANDLE;
    }
    g_vkPipelines.pipelineCache.clear();

    // Zero out runtime state (safe - contains only POD types)
    memset(&g_vkRunState, 0, sizeof(g_vkRunState));

    // Create descriptor set layouts
    VK_CreateDescriptorSetLayouts();

    // Create descriptor pool
    VK_CreateDescriptorPool();

    // Create pipeline layout
    VK_CreatePipelineLayout();

    // Load shader modules
    VK_LoadAllShaders();

    // Initialize runtime state
    g_vkRunState.viewVSDirty = qtrue;
    g_vkRunState.viewPSDirty = qtrue;
}

//----------------------------------------------------------------------------
// Destroy pipeline state
//----------------------------------------------------------------------------
void VK_DestroyPipelineState() {
    // Destroy all cached pipelines
    for (auto& pair : g_vkPipelines.pipelineCache) {
        if (pair.second != VK_NULL_HANDLE) {
            vkDestroyPipeline(g_vkDevice.device, pair.second, nullptr);
        }
    }
    g_vkPipelines.pipelineCache.clear();

    // Destroy shader modules
    VK_DestroyAllShaders();

    // Destroy pipeline layout
    if (g_vkPipelines.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_vkDevice.device, g_vkPipelines.pipelineLayout, nullptr);
        g_vkPipelines.pipelineLayout = VK_NULL_HANDLE;
    }

    // Destroy descriptor pool (also frees all descriptor sets)
    if (g_vkPipelines.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_vkDevice.device, g_vkPipelines.descriptorPool, nullptr);
        g_vkPipelines.descriptorPool = VK_NULL_HANDLE;
    }

    // Destroy descriptor set layouts
    for (int i = 0; i < 3; i++) {
        if (g_vkPipelines.setLayouts[i] != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(g_vkDevice.device, g_vkPipelines.setLayouts[i], nullptr);
            g_vkPipelines.setLayouts[i] = VK_NULL_HANDLE;
        }
    }
}

//----------------------------------------------------------------------------
// Get vertex input state for shader type
//----------------------------------------------------------------------------
void VK_GetVertexInputState(vkShaderType_t shaderType,
                            VkPipelineVertexInputStateCreateInfo* vertexInputInfo,
                            std::vector<VkVertexInputBindingDescription>& bindings,
                            std::vector<VkVertexInputAttributeDescription>& attributes) {

    bindings.clear();
    attributes.clear();

    if (shaderType == VK_SHADER_SINGLE_TEXTURE) {
        // Single-texture: 3 bindings (position, texcoord, color)
        bindings.resize(3);
        attributes.resize(3);

        // Binding 0: Position (vec4)
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(float) * 4;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[0].offset = 0;

        // Binding 1: TexCoord (vec2)
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(float) * 2;
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[1].location = 1;
        attributes[1].binding = 1;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = 0;

        // Binding 2: Color (color4ub_t / RGBA8)
        bindings[2].binding = 2;
        bindings[2].stride = 4;  // 4 bytes (RGBA)
        bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[2].location = 2;
        attributes[2].binding = 2;
        attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
        attributes[2].offset = 0;
    }
    else if (shaderType == VK_SHADER_MULTI_TEXTURE) {
        // Multi-texture: 4 bindings (position, texcoord0, texcoord1, color)
        bindings.resize(4);
        attributes.resize(4);

        // Binding 0: Position (vec4)
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(float) * 4;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[0].offset = 0;

        // Binding 1: TexCoord0 (vec2)
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(float) * 2;
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[1].location = 1;
        attributes[1].binding = 1;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = 0;

        // Binding 2: TexCoord1 (vec2)
        bindings[2].binding = 2;
        bindings[2].stride = sizeof(float) * 2;
        bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[2].location = 2;
        attributes[2].binding = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = 0;

        // Binding 3: Color (color4ub_t / RGBA8)
        bindings[3].binding = 3;
        bindings[3].stride = 4;
        bindings[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[3].location = 3;
        attributes[3].binding = 3;
        attributes[3].format = VK_FORMAT_R8G8B8A8_UNORM;
        attributes[3].offset = 0;
    }
    else if (shaderType == VK_SHADER_SKYBOX) {
        // Skybox: 2 bindings (position, texcoord)
        bindings.resize(2);
        attributes.resize(2);

        // Binding 0: Position (vec3)
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(float) * 3;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = 0;

        // Binding 1: TexCoord (vec2)
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(float) * 2;
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[1].location = 1;
        attributes[1].binding = 1;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = 0;
    }
    else if (shaderType == VK_SHADER_FSQ) {
        // Fullscreen quad / 2D: 2 bindings (position2D, texcoord)
        bindings.resize(2);
        attributes.resize(2);

        // Binding 0: Position (vec2)
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(float) * 2;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].offset = 0;

        // Binding 1: TexCoord (vec2)
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(float) * 2;
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[1].location = 1;
        attributes[1].binding = 1;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = 0;
    }

    vertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo->vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInputInfo->pVertexBindingDescriptions = bindings.data();
    vertexInputInfo->vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInputInfo->pVertexAttributeDescriptions = attributes.data();
}

//----------------------------------------------------------------------------
// Map GLS_* blend flags to Vulkan blend state
//----------------------------------------------------------------------------
void VK_MapBlendState(unsigned long stateMask, VkPipelineColorBlendAttachmentState* blendState) {
    // Extract blend bits
    unsigned long srcBlend = stateMask & GLS_SRCBLEND_BITS;
    unsigned long dstBlend = stateMask & GLS_DSTBLEND_BITS;

    // If both are zero, no blending (opaque)
    if (srcBlend == 0 && dstBlend == 0) {
        blendState->blendEnable = VK_FALSE;
        blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendState->colorBlendOp = VK_BLEND_OP_ADD;
        blendState->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendState->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendState->alphaBlendOp = VK_BLEND_OP_ADD;
        blendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        return;
    }

    // Enable blending
    blendState->blendEnable = VK_TRUE;

    // Map source blend factor
    switch (srcBlend) {
        case GLS_SRCBLEND_ZERO:                 blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ZERO; break;
        case GLS_SRCBLEND_ONE:                  blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE; break;
        case GLS_SRCBLEND_DST_COLOR:            blendState->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR; break;
        case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:  blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR; break;
        case GLS_SRCBLEND_SRC_ALPHA:            blendState->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; break;
        case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:  blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
        case GLS_SRCBLEND_DST_ALPHA:            blendState->srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA; break;
        case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:  blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
        case GLS_SRCBLEND_ALPHA_SATURATE:       blendState->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE; break;
        default:                                blendState->srcColorBlendFactor = VK_BLEND_FACTOR_ONE; break;
    }

    // Map destination blend factor
    switch (dstBlend) {
        case GLS_DSTBLEND_ZERO:                 blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; break;
        case GLS_DSTBLEND_ONE:                  blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE; break;
        case GLS_DSTBLEND_SRC_COLOR:            blendState->dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR; break;
        case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:  blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR; break;
        case GLS_DSTBLEND_SRC_ALPHA:            blendState->dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; break;
        case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:  blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
        case GLS_DSTBLEND_DST_ALPHA:            blendState->dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA; break;
        case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:  blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
        default:                                blendState->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; break;
    }

    // Use same blend factors for alpha
    blendState->srcAlphaBlendFactor = blendState->srcColorBlendFactor;
    blendState->dstAlphaBlendFactor = blendState->dstColorBlendFactor;

    blendState->colorBlendOp = VK_BLEND_OP_ADD;
    blendState->alphaBlendOp = VK_BLEND_OP_ADD;
    blendState->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}

//----------------------------------------------------------------------------
// Map GLS_* depth flags to Vulkan depth/stencil state
//----------------------------------------------------------------------------
void VK_MapDepthState(unsigned long stateMask, VkPipelineDepthStencilStateCreateInfo* depthState) {
    depthState->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    // Depth test enable
    depthState->depthTestEnable = !(stateMask & GLS_DEPTHTEST_DISABLE) ? VK_TRUE : VK_FALSE;

    // Depth write enable
    depthState->depthWriteEnable = (stateMask & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;

    // Depth compare op
    if (stateMask & GLS_DEPTHFUNC_EQUAL) {
        depthState->depthCompareOp = VK_COMPARE_OP_EQUAL;
    } else {
        depthState->depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    }

    depthState->depthBoundsTestEnable = VK_FALSE;
    depthState->stencilTestEnable = VK_FALSE;  // TODO: Stencil shadows
    depthState->minDepthBounds = 0.0f;
    depthState->maxDepthBounds = 1.0f;
}

//----------------------------------------------------------------------------
// Map cull mode to Vulkan rasterization state
//----------------------------------------------------------------------------
void VK_MapRasterState(int cullMode, unsigned long stateMask, VkPipelineRasterizationStateCreateInfo* rasterState) {
    rasterState->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterState->depthClampEnable = VK_FALSE;
    rasterState->rasterizerDiscardEnable = VK_FALSE;

    // Polygon mode (wireframe for debug)
    if (stateMask & GLS_POLYMODE_LINE) {
        rasterState->polygonMode = VK_POLYGON_MODE_LINE;
    } else {
        rasterState->polygonMode = VK_POLYGON_MODE_FILL;
    }

    rasterState->lineWidth = 1.0f;

    // Cull mode
    switch (cullMode) {
        case CT_FRONT_SIDED:
            rasterState->cullMode = VK_CULL_MODE_BACK_BIT;
            rasterState->frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
        case CT_BACK_SIDED:
            rasterState->cullMode = VK_CULL_MODE_FRONT_BIT;
            rasterState->frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
        case CT_TWO_SIDED:
        default:
            rasterState->cullMode = VK_CULL_MODE_NONE;
            rasterState->frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
    }

    // Polygon offset (not supported in engine's state bits)
    rasterState->depthBiasEnable = VK_FALSE;
    rasterState->depthBiasConstantFactor = 0.0f;
    rasterState->depthBiasClamp = 0.0f;
    rasterState->depthBiasSlopeFactor = 0.0f;
}

//----------------------------------------------------------------------------
// Create pipeline from key
//----------------------------------------------------------------------------
VkPipeline VK_CreatePipeline(const vkPipelineKey_t& key) {
    vkShaderType_t shaderType = (vkShaderType_t)key.shaderType;

    // Get shader modules
    VkShaderModule vertShader = VK_GetShaderModule(shaderType, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragShader = VK_GetShaderModule(shaderType, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (vertShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "WARNING: Missing shader modules for shader type %d\n", shaderType);
        return VK_NULL_HANDLE;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = VK_CreateShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main");
    shaderStages[1] = VK_CreateShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main");

    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    VK_GetVertexInputState(shaderType, &vertexInputInfo, bindings, attributes);

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    VK_MapRasterState(key.cullMode, key.depthFlags, &rasterizer);

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = (VkSampleCountFlagBits)key.sampleCount;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 1.0f;

    // Depth/stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    VK_MapDepthState(key.depthFlags, &depthStencil);

    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    VK_MapBlendState(key.blendSrc | key.blendDst, &colorBlendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 3;
    dynamicState.pDynamicStates = dynamicStates;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = g_vkPipelines.pipelineLayout;
    pipelineInfo.renderPass = g_vkDevice.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(g_vkDevice.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "WARNING: Failed to create pipeline: 0x%08X\n", result);
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

//----------------------------------------------------------------------------
// Get or create pipeline (with caching)
//----------------------------------------------------------------------------
VkPipeline VK_GetOrCreatePipeline(const vkPipelineKey_t& key) {
    // Check cache
    auto it = g_vkPipelines.pipelineCache.find(key);
    if (it != g_vkPipelines.pipelineCache.end()) {
        return it->second;
    }

    // Create new pipeline
    VkPipeline pipeline = VK_CreatePipeline(key);

    if (pipeline != VK_NULL_HANDLE) {
        // Add to cache
        g_vkPipelines.pipelineCache[key] = pipeline;
    }

    return pipeline;
}
