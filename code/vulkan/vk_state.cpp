// Vulkan rendering backend - state management implementation

#include "vk_state.h"
#include "vk_shaders.h"
#include "vk_buffers.h"
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

// VS UBO layout (matches ViewDataVS in GLSL shaders):
// mat4 Projection;      // 64 bytes (offset 0)
// mat4 View;            // 64 bytes (offset 64)
// vec2 DepthRange;      // 8 bytes (offset 128)
// padding;              // 8 bytes (offset 136) - align vec3 to 16 bytes
// vec3 EyePos;          // 16 bytes (offset 144) - vec3 in std140 takes 16 bytes
// Total: 160 bytes (aligned to 256 for UBO alignment requirements)
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
    float _padding1[2];   // align eyePos to 16 bytes (std140 requirement for vec3)
    float eyePos[4];      // vec3 + padding (std140: vec3 takes 16 bytes)
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
    qboolean isMirror;
    qboolean isMultitextured;
    qboolean isSkybox;
    VkPipeline pipeline;
} vkCachedPipeline_t;

static vkCachedPipeline_t s_pipelineCache[MAX_CACHED_PIPELINES];

//=============================================================================
// Culling helpers
//=============================================================================

// Convert Q3 cull type to Vulkan cull mode, accounting for mirror rendering
static VkCullModeFlags GetVkCullMode(int cullType, qboolean isMirror) {
    if (cullType == CT_TWO_SIDED) {
        return VK_CULL_MODE_NONE;
    }

    if (cullType == CT_BACK_SIDED) {
        // Cull back faces, but flip for mirrors (matches D3D11)
        if (isMirror) {
            return VK_CULL_MODE_FRONT_BIT;
        } else {
            return VK_CULL_MODE_BACK_BIT;
        }
    } else {
        // CT_FRONT_SIDED: Cull front faces, but flip for mirrors
        if (isMirror) {
            return VK_CULL_MODE_BACK_BIT;
        } else {
            return VK_CULL_MODE_FRONT_BIT;
        }
    }
}
static int s_pipelineCacheCount = 0;

// Dedicated 2D pipeline (different vertex format)
static VkPipeline s_2dPipeline = VK_NULL_HANDLE;

// Dedicated skybox pipeline (different vertex format: position[3] + texcoord[2])
static VkPipeline s_skyboxPipeline = VK_NULL_HANDLE;

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

    // Initialize alpha clip to "no alpha test" state (matching D3D11)
    // alphaClip[0] = 1, alphaClip[1] = 0 means "clip if alpha < 0" which passes everything
    g_vkViewState.alphaClip[0] = 1.0f;
    g_vkViewState.alphaClip[1] = 0.0f;

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

        // Binding 0: VS Uniform Buffer (DYNAMIC - offset set at bind time for per-draw data)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        // Binding 1: PS Uniform Buffer (DYNAMIC - offset set at bind time for per-draw data)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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

    // Destroy skybox pipeline
    if (s_skyboxPipeline) {
        qvkDestroyPipeline(vk.device, s_skyboxPipeline, NULL);
        s_skyboxPipeline = VK_NULL_HANDLE;
    }

    // Destroy cached pipelines
    for (int i = 0; i < s_pipelineCacheCount; i++) {
        if (s_pipelineCache[i].pipeline) {
            qvkDestroyPipeline(vk.device, s_pipelineCache[i].pipeline, NULL);
        }
    }
    // Clear the entire cache array to prevent stale handles
    Com_Memset(s_pipelineCache, 0, sizeof(s_pipelineCache));
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
            // No alpha test - use threshold 0 so nothing gets clipped (alpha always >= 0)
            // Must match D3D11: alphaClip[0] = 1, alphaClip[1] = 0
            g_vkViewState.alphaClip[0] = 1.0f;
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
            // Unknown alpha test mode - treat as no alpha test (same as case 0)
            g_vkViewState.alphaClip[0] = 1.0f;
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

void VkState_SetEyePos(const float* eyePos)
{
    g_vkViewState.eyePos[0] = eyePos[0];
    g_vkViewState.eyePos[1] = eyePos[1];
    g_vkViewState.eyePos[2] = eyePos[2];
}

//=============================================================================
// Portal rendering
//=============================================================================

void VkState_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane)
{
    g_vkPipelineState.portalRendering = enabled;

    if (enabled && plane && flipMatrix) {
        // Transform plane by flip matrix (matches D3D11)
        g_vkViewState.clipPlane[0] = flipMatrix[ 0] * plane[0] + flipMatrix[ 4] * plane[1] + flipMatrix[ 8] * plane[2] + flipMatrix[12] * plane[3];
        g_vkViewState.clipPlane[1] = flipMatrix[ 1] * plane[0] + flipMatrix[ 5] * plane[1] + flipMatrix[ 9] * plane[2] + flipMatrix[13] * plane[3];
        g_vkViewState.clipPlane[2] = flipMatrix[ 2] * plane[0] + flipMatrix[ 6] * plane[1] + flipMatrix[10] * plane[2] + flipMatrix[14] * plane[3];
        g_vkViewState.clipPlane[3] = flipMatrix[ 3] * plane[0] + flipMatrix[ 7] * plane[1] + flipMatrix[11] * plane[2] + flipMatrix[15] * plane[3];
        g_vkViewState.clipPlaneDirty = qtrue;
    } else {
        // Clear clip plane when portal rendering is disabled (matches D3D11)
        g_vkViewState.clipPlane[0] = 0.0f;
        g_vkViewState.clipPlane[1] = 0.0f;
        g_vkViewState.clipPlane[2] = 0.0f;
        g_vkViewState.clipPlane[3] = 0.0f;
        g_vkViewState.clipPlaneDirty = qtrue;
    }
}

//=============================================================================
// 2D/3D mode resets
//=============================================================================

void VkState_Reset2D(void)
{
    // Setup for 2D rendering (match D3D11 behavior)

    // Set ModelView to identity - 2D vertices are already in screen space
    static const float identityMatrix[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    VkState_SetModelView(identityMatrix);

    // Set state bits for 2D: no depth test, alpha blending
    g_vkPipelineState.cullMode = CT_TWO_SIDED;
    g_vkPipelineState.stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

    // Disable portal rendering
    VkState_SetPortalRendering(qfalse, NULL, NULL);

    // Set depth range to 0,0 for 2D (everything at front)
    VkState_SetDepthRange(0, 0);
}

void VkState_Reset3D(void)
{
    // Setup for 3D rendering (match D3D11 behavior)

    // Set ModelView to identity - actual matrix will be set per-view
    static const float identityMatrix[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    VkState_SetModelView(identityMatrix);

    // Set default state bits for 3D
    g_vkPipelineState.stateBits = GLS_DEFAULT;

    // Set full depth range for 3D
    VkState_SetDepthRange(0, 1);
}

//=============================================================================
// Pipeline creation/caching
//=============================================================================

// Get blend factor for color blending
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

// Get blend factor for alpha blending (maps color factors to alpha equivalents, like D3D11)
static VkBlendFactor GetBlendFactorAlpha(unsigned long bits, qboolean isSrc)
{
    unsigned long mask = isSrc ? GLS_SRCBLEND_BITS : GLS_DSTBLEND_BITS;
    unsigned long factor = bits & mask;

    if (isSrc) {
        switch (factor) {
            case GLS_SRCBLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
            case GLS_SRCBLEND_ONE: return VK_BLEND_FACTOR_ONE;
            case GLS_SRCBLEND_DST_COLOR: return VK_BLEND_FACTOR_DST_ALPHA;  // Map to alpha
            case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;  // Map to alpha
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
            case GLS_DSTBLEND_SRC_COLOR: return VK_BLEND_FACTOR_SRC_ALPHA;  // Map to alpha
            case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // Map to alpha
            case GLS_DSTBLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
            case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case GLS_DSTBLEND_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
            case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            default: return VK_BLEND_FACTOR_ZERO;
        }
    }
}

static VkPipeline CreatePipeline(unsigned long stateBits, int cullMode, qboolean isMirror,
                                  qboolean isMultitextured, qboolean isSkybox)
{
    // Validate Vulkan state before creating pipeline
    if (!vk.device || !vk.renderPass || !s_pipelineLayout) {
        Com_Printf("ERROR: CreatePipeline called before Vulkan fully initialized\n");
        return VK_NULL_HANDLE;
    }

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
    // Only include TexCoord1 (location 3) for multitextured shaders
    vertexInputInfo.vertexAttributeDescriptionCount = isMultitextured ? 4 : 3;
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

    // Note: Vulkan uses negative viewport height for Y flip. This reverses winding order.
    // Q3 geometry is CCW front-facing. After Y flip, it appears CW on screen.
    // Keep CCW as front face, but invert which face we cull.
    rasterizer.cullMode = GetVkCullMode(cullMode, isMirror);
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;  // Dynamic state - set via vkCmdSetDepthBias

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk.msaaSamples;

    // Depth stencil - derive from stateBits (matching D3D11 behavior)
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = (stateBits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
    depthStencil.depthWriteEnable = (stateBits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = (stateBits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
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
        // Use alpha-mapped blend factors (like D3D11) for proper layered transparency
        colorBlendAttachment.srcAlphaBlendFactor = GetBlendFactorAlpha(stateBits, qtrue);
        colorBlendAttachment.dstAlphaBlendFactor = GetBlendFactorAlpha(stateBits, qfalse);
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

VkPipeline VkState_GetPipeline(unsigned long stateBits, int cullMode, qboolean isMirror,
                               qboolean isMultitextured, qboolean isSkybox)
{
    // Ensure Vulkan is initialized
    if (!vk.device || !vk.initialized) {
        return VK_NULL_HANDLE;
    }

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
            s_pipelineCache[i].isMirror == isMirror &&
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

    VkPipeline pipeline = CreatePipeline(relevantBits, cullMode, isMirror, isMultitextured, isSkybox);
    if (pipeline) {
        s_pipelineCache[s_pipelineCacheCount].stateBits = relevantBits;
        s_pipelineCache[s_pipelineCacheCount].cullMode = cullMode;
        s_pipelineCache[s_pipelineCacheCount].isMirror = isMirror;
        s_pipelineCache[s_pipelineCacheCount].isMultitextured = isMultitextured;
        s_pipelineCache[s_pipelineCacheCount].isSkybox = isSkybox;
        s_pipelineCache[s_pipelineCacheCount].pipeline = pipeline;
        s_pipelineCacheCount++;
    }

    return pipeline;
}

static VkPipeline Create2DPipeline(void)
{
    // Validate Vulkan state before creating pipeline
    if (!vk.device || !vk.renderPass || !s_pipelineLayout) {
        Com_Printf("ERROR: Create2DPipeline called before Vulkan fully initialized\n");
        return VK_NULL_HANDLE;
    }

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
    multisampling.rasterizationSamples = vk.msaaSamples;

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
    // Ensure Vulkan is initialized
    if (!vk.device || !vk.initialized) {
        return VK_NULL_HANDLE;
    }

    if (!s_2dPipeline) {
        s_2dPipeline = Create2DPipeline();
    }
    return s_2dPipeline;
}

static VkPipeline CreateSkyboxPipeline(void)
{
    // Validate Vulkan state before creating pipeline
    if (!vk.device || !vk.renderPass || !s_pipelineLayout) {
        Com_Printf("ERROR: CreateSkyboxPipeline called before Vulkan fully initialized\n");
        return VK_NULL_HANDLE;
    }

    // Get skybox shaders
    VkShaderModule vertShader = VkShaders_GetVertexShader(VK_SHADER_SKYBOX);
    VkShaderModule fragShader = VkShaders_GetFragmentShader(VK_SHADER_SKYBOX);

    if (!vertShader || !fragShader) {
        Com_Printf("ERROR: Failed to get skybox shaders\n");
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

    // Skybox Vertex layout: position[3] + texCoord[2] = 20 bytes
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 5;  // position[3] + texcoord[2]
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2] = {};
    // Position - location 0 (shader expects vec4, Vulkan auto-fills w=1.0)
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = 0;
    // TexCoord - location 1
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = sizeof(float) * 3;

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

    // Rasterization - two-sided (no culling) for skybox
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Two-sided for skybox
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk.msaaSamples;

    // Depth stencil - depth read only (no writes) for skybox
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;  // Don't write depth
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending - no blending for skybox
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

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
        Com_Printf("ERROR: vkCreateGraphicsPipelines failed for skybox pipeline: %s\n", Vk_ResultString(result));
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

VkPipeline VkState_GetSkyboxPipeline(void)
{
    // Ensure Vulkan is initialized
    if (!vk.device || !vk.initialized) {
        return VK_NULL_HANDLE;
    }

    if (!s_skyboxPipeline) {
        s_skyboxPipeline = CreateSkyboxPipeline();
    }
    return s_skyboxPipeline;
}

void VkState_ResetPipelines(void)
{
    // Wait for GPU to finish using any pipelines
    if (vk.device) {
        qvkDeviceWaitIdle(vk.device);
    }

    // Destroy 2D pipeline
    if (s_2dPipeline) {
        qvkDestroyPipeline(vk.device, s_2dPipeline, NULL);
        s_2dPipeline = VK_NULL_HANDLE;
    }

    // Destroy skybox pipeline
    if (s_skyboxPipeline) {
        qvkDestroyPipeline(vk.device, s_skyboxPipeline, NULL);
        s_skyboxPipeline = VK_NULL_HANDLE;
    }

    // Destroy cached pipelines
    for (int i = 0; i < s_pipelineCacheCount; i++) {
        if (s_pipelineCache[i].pipeline) {
            qvkDestroyPipeline(vk.device, s_pipelineCache[i].pipeline, NULL);
        }
    }
    Com_Memset(s_pipelineCache, 0, sizeof(s_pipelineCache));
    s_pipelineCacheCount = 0;
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

void VkState_UpdateUniforms(void)
{
    // Always update VS uniform buffer every frame (HOST_COHERENT, no flush needed)
    // Previous bug: dirty flags could prevent initial/subsequent updates
    if (s_vsUniformMapped) {
        vsUniformData_t* vsData = (vsUniformData_t*)s_vsUniformMapped;

        // Copy matrices - shader expects column_major (matches OpenGL/Q3 convention)
        memcpy(vsData->projection, g_vkViewState.projectionMatrix, sizeof(float) * 16);
        memcpy(vsData->view, g_vkViewState.modelViewMatrix, sizeof(float) * 16);

        // Match D3D11's depth range formula - NDC conversion is done in shader
        // D3D11 passes (minRange, maxRange - minRange), shader handles OpenGL->Vulkan NDC conversion
        float minRange = g_vkViewState.depthRange[0];
        float maxRange = g_vkViewState.depthRange[1];
        vsData->depthRange[0] = minRange;
        vsData->depthRange[1] = maxRange - minRange;

        // Copy eye position for skybox centering
        vsData->eyePos[0] = g_vkViewState.eyePos[0];
        vsData->eyePos[1] = g_vkViewState.eyePos[1];
        vsData->eyePos[2] = g_vkViewState.eyePos[2];
        vsData->eyePos[3] = 0.0f;  // padding
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

//=============================================================================
// Dynamic UBO allocation (per-draw uniforms from ring buffer)
//=============================================================================

qboolean VkState_AllocDynamicUniforms(uint32_t* outVsOffset, uint32_t* outPsOffset)
{
    // Allocate VS uniform data from ring buffer
    vkBufferAlloc_t vsAlloc = VkBuffers_AllocUniform(VS_UBO_SIZE);
    if (!vsAlloc.data) {
        return qfalse;
    }

    // Allocate PS uniform data from ring buffer
    vkBufferAlloc_t psAlloc = VkBuffers_AllocUniform(PS_UBO_SIZE);
    if (!psAlloc.data) {
        return qfalse;
    }

    // Copy VS uniform data (matrices and other state)
    vsUniformData_t* vsData = (vsUniformData_t*)vsAlloc.data;
    memcpy(vsData->projection, g_vkViewState.projectionMatrix, sizeof(float) * 16);
    memcpy(vsData->view, g_vkViewState.modelViewMatrix, sizeof(float) * 16);
    // Match D3D11's depth range formula - NDC conversion is done in shader
    // D3D11 passes (minRange, maxRange - minRange), shader handles OpenGL->Vulkan NDC conversion
    {
        float minRange = g_vkViewState.depthRange[0];
        float maxRange = g_vkViewState.depthRange[1];
        vsData->depthRange[0] = minRange;
        vsData->depthRange[1] = maxRange - minRange;
    }
    vsData->eyePos[0] = g_vkViewState.eyePos[0];
    vsData->eyePos[1] = g_vkViewState.eyePos[1];
    vsData->eyePos[2] = g_vkViewState.eyePos[2];
    vsData->eyePos[3] = 0.0f;

    // Copy PS uniform data (clip plane and alpha test)
    psUniformData_t* psData = (psUniformData_t*)psAlloc.data;
    memcpy(psData->clipPlane, g_vkViewState.clipPlane, sizeof(float) * 4);
    psData->alphaClip[0] = g_vkViewState.alphaClip[0];
    psData->alphaClip[1] = g_vkViewState.alphaClip[1];

    // Return offsets for dynamic binding
    *outVsOffset = (uint32_t)vsAlloc.offset;
    *outPsOffset = (uint32_t)psAlloc.offset;

    return qtrue;
}
