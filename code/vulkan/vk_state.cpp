// Vulkan rendering backend - state management implementation

#include "vk_state.h"
#include "vk_shaders.h"
#include <string.h>

//=============================================================================
// Globals
//=============================================================================

vkViewState_t g_vkViewState;
vkPipelineState_t g_vkPipelineState;

// Descriptor set layouts
static VkDescriptorSetLayout s_textureSetLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_uniformSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;

//=============================================================================
// Uniform Buffer Objects
//=============================================================================

// VS UBO layout (matches ViewDataVS in vscommon.h):
// float4x4 Projection;  // 64 bytes
// float4x4 View;        // 64 bytes
// float2 DepthRange;    // 8 bytes + 8 padding
// Total: 144 bytes (aligned to 256 for UBO alignment requirements)
#define VS_UBO_SIZE 256

// PS UBO layout (matches ViewDataPS in pscommon.h):
// float4 c_ClipPlane;   // 16 bytes
// float2 c_AlphaClip;   // 8 bytes + 8 padding
// Total: 32 bytes (aligned to 256)
#define PS_UBO_SIZE 256

typedef struct vsUniformData_s {
    float projection[16];
    float view[16];
    float depthRange[2];
    float _padding[2];
} vsUniformData_t;

typedef struct psUniformData_s {
    float clipPlane[4];
    float alphaClip[2];
    float _padding[2];
} psUniformData_t;

static VkBuffer s_vsUniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory s_vsUniformMemory = VK_NULL_HANDLE;
static void* s_vsUniformMapped = NULL;

static VkBuffer s_psUniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory s_psUniformMemory = VK_NULL_HANDLE;
static void* s_psUniformMapped = NULL;

// Pipeline cache (hash map would be better, but simple array for now)
#define MAX_CACHED_PIPELINES 256

typedef struct vkCachedPipeline_s {
    unsigned long stateBits;
    int cullMode;
    qboolean isMultitextured;
    qboolean isSkybox;
    VkPipeline pipeline;
} vkCachedPipeline_t;

static vkCachedPipeline_t s_pipelineCache[MAX_CACHED_PIPELINES];
static int s_pipelineCacheCount = 0;

// Dedicated 2D pipeline (different vertex format)
static VkPipeline s_2dPipeline = VK_NULL_HANDLE;

//=============================================================================
// Initialization
//=============================================================================

static qboolean CreateUniformBuffer(VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void** mapped)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = qvkCreateBuffer(vk.device, &bufferInfo, NULL, buffer);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: Failed to create uniform buffer: %s\n", Vk_ResultString(result));
        return qfalse;
    }

    VkMemoryRequirements memReqs;
    qvkGetBufferMemoryRequirements(vk.device, *buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = Vk_FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, memory);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: Failed to allocate uniform buffer memory: %s\n", Vk_ResultString(result));
        qvkDestroyBuffer(vk.device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return qfalse;
    }

    qvkBindBufferMemory(vk.device, *buffer, *memory, 0);

    // Map persistently
    qvkMapMemory(vk.device, *memory, 0, size, 0, mapped);

    return qtrue;
}

static void DestroyUniformBuffer(VkBuffer* buffer, VkDeviceMemory* memory, void** mapped)
{
    if (*mapped) {
        qvkUnmapMemory(vk.device, *memory);
        *mapped = NULL;
    }
    if (*memory) {
        qvkFreeMemory(vk.device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
    if (*buffer) {
        qvkDestroyBuffer(vk.device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
    }
}

void VkState_Init(void)
{
    Com_Memset(&g_vkViewState, 0, sizeof(g_vkViewState));
    Com_Memset(&g_vkPipelineState, 0, sizeof(g_vkPipelineState));
    Com_Memset(s_pipelineCache, 0, sizeof(s_pipelineCache));
    s_pipelineCacheCount = 0;

    // Set identity matrices
    for (int i = 0; i < 16; i++) {
        g_vkViewState.projectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        g_vkViewState.modelViewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    g_vkViewState.depthRange[0] = 0.0f;
    g_vkViewState.depthRange[1] = 1.0f;

    // Initialize clip plane to disabled (all zeros - shader checks for this)
    g_vkViewState.clipPlane[0] = 0.0f;
    g_vkViewState.clipPlane[1] = 0.0f;
    g_vkViewState.clipPlane[2] = 0.0f;
    g_vkViewState.clipPlane[3] = 0.0f;

    // Initialize alpha clip to disabled state
    // alphaClip[0] = enable flag (0 = disabled, 1 = enabled)
    // alphaClip[1] = threshold value
    g_vkViewState.alphaClip[0] = 0.0f;  // Disabled
    g_vkViewState.alphaClip[1] = 0.0f;  // Threshold (unused when disabled)

    g_vkPipelineState.depthMin = 0.0f;
    g_vkPipelineState.depthMax = 1.0f;

    // Create uniform buffers
    if (!CreateUniformBuffer(VS_UBO_SIZE, &s_vsUniformBuffer, &s_vsUniformMemory, &s_vsUniformMapped)) {
        Com_Printf("WARNING: Failed to create VS uniform buffer\n");
    }
    if (!CreateUniformBuffer(PS_UBO_SIZE, &s_psUniformBuffer, &s_psUniformMemory, &s_psUniformMapped)) {
        Com_Printf("WARNING: Failed to create PS uniform buffer\n");
    }

    // Mark uniforms as dirty so they get uploaded
    g_vkViewState.projectionDirty = qtrue;
    g_vkViewState.modelViewDirty = qtrue;
    g_vkViewState.clipPlaneDirty = qtrue;
    g_vkViewState.alphaClipDirty = qtrue;

    // Create descriptor set layouts
    // Main set layout - matches HLSL shader bindings (all in Set 0):
    // Single-texture shaders (compile_spirv.bat: -fvk-t-shift 2, -fvk-s-shift 3):
    //   Binding 0: VS Uniform Buffer (ViewDataVS)
    //   Binding 1: PS Uniform Buffer (ViewDataPS)
    //   Binding 2: Texture (Diffuse) - sampled image
    //   Binding 3: Sampler
    // Multi-texture shaders (compile_spirv.bat: -fvk-t-shift 2, -fvk-s-shift 4):
    //   Binding 0: VS Uniform Buffer (ViewDataVS)
    //   Binding 1: PS Uniform Buffer (ViewDataPS)
    //   Binding 2: Texture (Diffuse) - sampled image
    //   Binding 3: Texture (Lightmap) - sampled image
    //   Binding 4: Sampler
    // We create a superset layout that works for all shaders
    {
        VkDescriptorSetLayoutBinding bindings[5] = {};

        // Binding 0: VS Uniform Buffer
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        // Binding 1: PS Uniform Buffer
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 2: Diffuse texture (sampled image, separate from sampler)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 3: Lightmap texture (for multi-texture) or Sampler (for single-texture)
        // Since single-texture uses sampler at 3 and multi-texture uses texture at 3,
        // we need to make this binding work for both. Using combined allows either.
        // Actually, let's create separate layouts or use partially bound descriptors.
        // For simplicity: binding 3 = second texture (SAMPLED_IMAGE)
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 4: Sampler (used by multi-texture at 4, single-texture at 3)
        // We'll use binding 4 for sampler and fix single-texture to also use 4
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 5;
        layoutInfo.pBindings = bindings;

        qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &s_textureSetLayout);
    }

    // Uniform set layout not needed separately - all in main set
    s_uniformSetLayout = VK_NULL_HANDLE;

    // Pipeline layout - single descriptor set
    {
        VkDescriptorSetLayout setLayouts[] = { s_textureSetLayout };

        // Push constant range for per-draw data
        VkPushConstantRange pushConstant = {};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(float) * 20; // MVP + color + misc

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        qvkCreatePipelineLayout(vk.device, &layoutInfo, NULL, &s_pipelineLayout);
    }
}

void VkState_Shutdown(void)
{
    // Destroy uniform buffers
    DestroyUniformBuffer(&s_vsUniformBuffer, &s_vsUniformMemory, &s_vsUniformMapped);
    DestroyUniformBuffer(&s_psUniformBuffer, &s_psUniformMemory, &s_psUniformMapped);

    // Destroy 2D pipeline
    if (s_2dPipeline) {
        qvkDestroyPipeline(vk.device, s_2dPipeline, NULL);
        s_2dPipeline = VK_NULL_HANDLE;
    }

    // Destroy cached pipelines
    for (int i = 0; i < s_pipelineCacheCount; i++) {
        if (s_pipelineCache[i].pipeline) {
            qvkDestroyPipeline(vk.device, s_pipelineCache[i].pipeline, NULL);
        }
    }
    s_pipelineCacheCount = 0;

    if (s_pipelineLayout) {
        qvkDestroyPipelineLayout(vk.device, s_pipelineLayout, NULL);
        s_pipelineLayout = VK_NULL_HANDLE;
    }

    if (s_textureSetLayout) {
        qvkDestroyDescriptorSetLayout(vk.device, s_textureSetLayout, NULL);
        s_textureSetLayout = VK_NULL_HANDLE;
    }

    if (s_uniformSetLayout) {
        qvkDestroyDescriptorSetLayout(vk.device, s_uniformSetLayout, NULL);
        s_uniformSetLayout = VK_NULL_HANDLE;
    }
}

//=============================================================================
// State bits
//=============================================================================

void VkState_SetState(unsigned long stateBits)
{
    unsigned long diff = stateBits ^ g_vkPipelineState.stateBits;
    g_vkPipelineState.stateBits = stateBits;

    // Update alpha clip state based on state bits (matching D3D11 behavior)
    // Alpha test: clip() will kill any alpha < threshold
    if (diff & GLS_ATEST_BITS)
    {
        const float alphaEps = 0.00001f; // Small epsilon for GT_0 test

        switch (stateBits & GLS_ATEST_BITS)
        {
        case 0:
            // No alpha test - disable alpha clipping
            g_vkViewState.alphaClip[0] = 0.0f;  // Disabled
            g_vkViewState.alphaClip[1] = 0.0f;
            break;
        case GLS_ATEST_GT_0:
            // Pass if alpha > 0 (basically any non-zero alpha)
            g_vkViewState.alphaClip[0] = 1.0f;  // Enabled
            g_vkViewState.alphaClip[1] = alphaEps;  // Very small threshold
            break;
        case GLS_ATEST_LT_80:
            // Pass if alpha < 0.5 (128/255 ~ 0.5)
            // Use negative flag to invert test
            g_vkViewState.alphaClip[0] = -1.0f;  // Enabled, inverted
            g_vkViewState.alphaClip[1] = 0.5f;
            break;
        case GLS_ATEST_GE_80:
            // Pass if alpha >= 0.5
            g_vkViewState.alphaClip[0] = 1.0f;  // Enabled
            g_vkViewState.alphaClip[1] = 0.5f;
            break;
        default:
            // Unknown alpha test mode - disable
            g_vkViewState.alphaClip[0] = 0.0f;
            g_vkViewState.alphaClip[1] = 0.0f;
            break;
        }
        g_vkViewState.alphaClipDirty = qtrue;
    }
}

unsigned long VkState_GetState(void)
{
    return g_vkPipelineState.stateBits;
}

//=============================================================================
// Matrices
//=============================================================================

void VkState_SetProjection(const float* matrix)
{
    memcpy(g_vkViewState.projectionMatrix, matrix, sizeof(float) * 16);
    g_vkViewState.projectionDirty = qtrue;
}

void VkState_GetProjection(float* matrix)
{
    memcpy(matrix, g_vkViewState.projectionMatrix, sizeof(float) * 16);
}

void VkState_SetModelView(const float* matrix)
{
    memcpy(g_vkViewState.modelViewMatrix, matrix, sizeof(float) * 16);
    g_vkViewState.modelViewDirty = qtrue;
}

void VkState_GetModelView(float* matrix)
{
    memcpy(matrix, g_vkViewState.modelViewMatrix, sizeof(float) * 16);
}

//=============================================================================
// Viewport
//=============================================================================

void VkState_SetViewport(int x, int y, int width, int height)
{
    g_vkPipelineState.viewportX = x;
    g_vkPipelineState.viewportY = y;
    g_vkPipelineState.viewportWidth = width;
    g_vkPipelineState.viewportHeight = height;
}

void VkState_SetDepthRange(float minRange, float maxRange)
{
    g_vkPipelineState.depthMin = minRange;
    g_vkPipelineState.depthMax = maxRange;
    g_vkViewState.depthRange[0] = minRange;
    g_vkViewState.depthRange[1] = maxRange;
}

//=============================================================================
// Portal rendering
//=============================================================================

void VkState_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane)
{
    g_vkPipelineState.portalRendering = enabled;

    if (enabled && plane) {
        memcpy(g_vkViewState.clipPlane, plane, sizeof(float) * 4);
        g_vkViewState.clipPlaneDirty = qtrue;
    }
}

//=============================================================================
// 2D/3D mode resets
//=============================================================================

void VkState_Reset2D(void)
{
    // Setup for 2D rendering
    g_vkPipelineState.cullMode = CT_TWO_SIDED;
    g_vkPipelineState.stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

    // Ortho projection for 2D
    // Will be set by the caller
}

void VkState_Reset3D(void)
{
    // Setup for 3D rendering
    g_vkPipelineState.stateBits = GLS_DEFAULT;
}

//=============================================================================
// Pipeline creation/caching
//=============================================================================

static VkBlendFactor GetBlendFactor(unsigned long bits, qboolean isSrc)
{
    unsigned long mask = isSrc ? GLS_SRCBLEND_BITS : GLS_DSTBLEND_BITS;
    unsigned long factor = bits & mask;

    if (isSrc) {
        switch (factor) {
            case GLS_SRCBLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
            case GLS_SRCBLEND_ONE: return VK_BLEND_FACTOR_ONE;
            case GLS_SRCBLEND_DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
            case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case GLS_SRCBLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
            case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case GLS_SRCBLEND_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
            case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case GLS_SRCBLEND_ALPHA_SATURATE: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            default: return VK_BLEND_FACTOR_ONE;
        }
    } else {
        switch (factor) {
            case GLS_DSTBLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
            case GLS_DSTBLEND_ONE: return VK_BLEND_FACTOR_ONE;
            case GLS_DSTBLEND_SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
            case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case GLS_DSTBLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
            case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case GLS_DSTBLEND_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
            case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            default: return VK_BLEND_FACTOR_ZERO;
        }
    }
}

static VkPipeline CreatePipeline(unsigned long stateBits, int cullMode,
                                  qboolean isMultitextured, qboolean isSkybox)
{
    // Get shaders
    VkShaderModule vertShader, fragShader;
    if (isSkybox) {
        vertShader = VkShaders_GetVertexShader(VK_SHADER_SKYBOX);
        fragShader = VkShaders_GetFragmentShader(VK_SHADER_SKYBOX);
    } else if (isMultitextured) {
        vertShader = VkShaders_GetVertexShader(VK_SHADER_GENERIC_MT);
        fragShader = VkShaders_GetFragmentShader(VK_SHADER_GENERIC_MT);
    } else {
        vertShader = VkShaders_GetVertexShader(VK_SHADER_GENERIC_ST);
        fragShader = VkShaders_GetFragmentShader(VK_SHADER_GENERIC_ST);
    }

    if (!vertShader || !fragShader) {
        return VK_NULL_HANDLE;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShader;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShader;
    shaderStages[1].pName = "main";

    // Vertex input
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 4 + sizeof(float) * 2 + sizeof(float) * 2 + sizeof(uint8_t) * 4;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Vertex layout matches vkGenericVertex_t:
    // float position[4];   offset 0,  size 16
    // float texCoord0[2];  offset 16, size 8
    // float texCoord1[2];  offset 24, size 8
    // byte color[4];       offset 32, size 4
    // Total stride: 36 bytes

    VkVertexInputAttributeDescription attrDescs[4] = {};
    // Position - location 0 (shader: POSITION)
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[0].offset = 0;
    // TexCoord0 - location 1 (shader: TEXCOORD0)
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = 16;
    // Color - location 2 (shader: COLOR)
    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrDescs[2].offset = 32;
    // TexCoord1 - location 3 (shader: TEXCOORD1, for multitexture)
    attrDescs[3].binding = 0;
    attrDescs[3].location = 3;
    attrDescs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[3].offset = 24;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

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

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = (stateBits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;

    // DEBUG: Force no culling to test if Y-flip winding is the issue
    (void)cullMode;  // Suppress unused warning
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // After Y flip
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // DEBUG: Temporarily disable depth test to see if that's the issue
    depthStencil.depthTestEnable = VK_FALSE; // (stateBits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE; // (stateBits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS; // (stateBits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    if ((stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) != 0) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = GetBlendFactor(stateBits, qtrue);
        colorBlendAttachment.dstColorBlendFactor = GetBlendFactor(stateBits, qfalse);
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = colorBlendAttachment.srcColorBlendFactor;
        colorBlendAttachment.dstAlphaBlendFactor = colorBlendAttachment.dstColorBlendFactor;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

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
    pipelineInfo.layout = s_pipelineLayout;
    pipelineInfo.renderPass = vk.renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    VkResult result = qvkCreateGraphicsPipelines(vk.device, vk.pipelineCache, 1, &pipelineInfo, NULL, &pipeline);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateGraphicsPipelines failed: %s\n", Vk_ResultString(result));
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

VkPipeline VkState_GetPipeline(unsigned long stateBits, int cullMode,
                               qboolean isMultitextured, qboolean isSkybox)
{
    // Mask out bits that don't affect pipeline state
    unsigned long relevantBits = stateBits & (
        GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS |
        GLS_DEPTHMASK_TRUE | GLS_POLYMODE_LINE |
        GLS_DEPTHTEST_DISABLE | GLS_DEPTHFUNC_EQUAL |
        GLS_ATEST_BITS
    );

    // Search cache
    for (int i = 0; i < s_pipelineCacheCount; i++) {
        if (s_pipelineCache[i].stateBits == relevantBits &&
            s_pipelineCache[i].cullMode == cullMode &&
            s_pipelineCache[i].isMultitextured == isMultitextured &&
            s_pipelineCache[i].isSkybox == isSkybox) {
            return s_pipelineCache[i].pipeline;
        }
    }

    // Create new pipeline
    if (s_pipelineCacheCount >= MAX_CACHED_PIPELINES) {
        Com_Printf("WARNING: Pipeline cache full\n");
        return VK_NULL_HANDLE;
    }

    VkPipeline pipeline = CreatePipeline(relevantBits, cullMode, isMultitextured, isSkybox);
    if (pipeline) {
        s_pipelineCache[s_pipelineCacheCount].stateBits = relevantBits;
        s_pipelineCache[s_pipelineCacheCount].cullMode = cullMode;
        s_pipelineCache[s_pipelineCacheCount].isMultitextured = isMultitextured;
        s_pipelineCache[s_pipelineCacheCount].isSkybox = isSkybox;
        s_pipelineCache[s_pipelineCacheCount].pipeline = pipeline;
        s_pipelineCacheCount++;
    }

    return pipeline;
}

static VkPipeline Create2DPipeline(void)
{
    // Get 2D shaders
    VkShaderModule vertShader = VkShaders_GetVertexShader(VK_SHADER_IMAGE2D);
    VkShaderModule fragShader = VkShaders_GetFragmentShader(VK_SHADER_IMAGE2D);

    if (!vertShader || !fragShader) {
        Com_Printf("ERROR: Failed to get 2D shaders\n");
        return VK_NULL_HANDLE;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShader;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShader;
    shaderStages[1].pName = "main";

    // 2D Vertex layout: vk2DVertex_t
    // float position[2];   offset 0,  size 8
    // float texCoord[2];   offset 8,  size 8
    // byte color[4];       offset 16, size 4
    // Total stride: 20 bytes
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = 20;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // image2d shader expects: float2 Position (loc 0), float2 TexCoord (loc 1)
    // No color input in shader
    VkVertexInputAttributeDescription attrDescs[2] = {};
    // Position - location 0
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[0].offset = 0;
    // TexCoord - location 1
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = 8;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

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

    // Rasterization - no culling for 2D
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil - disabled for 2D
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending - alpha blending for 2D
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
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
    pipelineInfo.layout = s_pipelineLayout;
    pipelineInfo.renderPass = vk.renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    VkResult result = qvkCreateGraphicsPipelines(vk.device, vk.pipelineCache, 1, &pipelineInfo, NULL, &pipeline);
    if (result != VK_SUCCESS) {
        Com_Printf("ERROR: vkCreateGraphicsPipelines failed for 2D pipeline: %s\n", Vk_ResultString(result));
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

VkPipeline VkState_Get2DPipeline(void)
{
    if (!s_2dPipeline) {
        s_2dPipeline = Create2DPipeline();
    }
    return s_2dPipeline;
}

//=============================================================================
// Getters
//=============================================================================

VkDescriptorSetLayout VkState_GetTextureSetLayout(void)
{
    return s_textureSetLayout;
}

VkDescriptorSetLayout VkState_GetUniformSetLayout(void)
{
    return s_uniformSetLayout;
}

VkPipelineLayout VkState_GetPipelineLayout(void)
{
    return s_pipelineLayout;
}

//=============================================================================
// UBO management
//=============================================================================

// Debug counter for uniform updates
static int s_uniformUpdateCount = 0;

void VkState_UpdateUniforms(void)
{
    // Always update VS uniform buffer every frame (HOST_COHERENT, no flush needed)
    // Previous bug: dirty flags could prevent initial/subsequent updates
    if (s_vsUniformMapped) {
        vsUniformData_t* vsData = (vsUniformData_t*)s_vsUniformMapped;

        // Copy matrices - shader expects column_major (matches OpenGL/Q3 convention)
        memcpy(vsData->projection, g_vkViewState.projectionMatrix, sizeof(float) * 16);
        memcpy(vsData->view, g_vkViewState.modelViewMatrix, sizeof(float) * 16);

        vsData->depthRange[0] = g_vkViewState.depthRange[0];
        vsData->depthRange[1] = g_vkViewState.depthRange[1] - g_vkViewState.depthRange[0];

        // Debug: print full matrix values first few times
        static int debugCount = 0;
        if (debugCount < 5) {
            debugCount++;
            Com_Printf("=== UBO Update %d ===\n", debugCount);
            Com_Printf("Projection matrix (column-major):\n");
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->projection[0], vsData->projection[4], vsData->projection[8], vsData->projection[12]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->projection[1], vsData->projection[5], vsData->projection[9], vsData->projection[13]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->projection[2], vsData->projection[6], vsData->projection[10], vsData->projection[14]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->projection[3], vsData->projection[7], vsData->projection[11], vsData->projection[15]);
            Com_Printf("ModelView matrix (column-major):\n");
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->view[0], vsData->view[4], vsData->view[8], vsData->view[12]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->view[1], vsData->view[5], vsData->view[9], vsData->view[13]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->view[2], vsData->view[6], vsData->view[10], vsData->view[14]);
            Com_Printf("  [%.3f, %.3f, %.3f, %.3f]\n", vsData->view[3], vsData->view[7], vsData->view[11], vsData->view[15]);
        }
    }

    // Update PS uniform buffer if needed
    if (s_psUniformMapped && (g_vkViewState.clipPlaneDirty || g_vkViewState.alphaClipDirty)) {
        psUniformData_t* psData = (psUniformData_t*)s_psUniformMapped;
        memcpy(psData->clipPlane, g_vkViewState.clipPlane, sizeof(float) * 4);
        psData->alphaClip[0] = g_vkViewState.alphaClip[0];
        psData->alphaClip[1] = g_vkViewState.alphaClip[1];

        g_vkViewState.clipPlaneDirty = qfalse;
        g_vkViewState.alphaClipDirty = qfalse;
    }
}

VkBuffer VkState_GetVSUniformBuffer(void)
{
    return s_vsUniformBuffer;
}

VkBuffer VkState_GetPSUniformBuffer(void)
{
    return s_psUniformBuffer;
}

VkDeviceSize VkState_GetVSUniformSize(void)
{
    return VS_UBO_SIZE;
}

VkDeviceSize VkState_GetPSUniformSize(void)
{
    return PS_UBO_SIZE;
}
