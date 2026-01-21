#pragma once

#include "vk_common.h"

//----------------------------------------------------------------------------
// Pipeline state constants
//----------------------------------------------------------------------------
enum vkShaderType_t {
    VK_SHADER_SINGLE_TEXTURE = 0,
    VK_SHADER_MULTI_TEXTURE = 1,
    VK_SHADER_SKYBOX = 2,
    VK_SHADER_FSQ = 3,
    VK_SHADER_COUNT = 4
};

//----------------------------------------------------------------------------
// Uniform buffer structures (must match GLSL layouts)
//----------------------------------------------------------------------------
struct vkViewVSUniform_t {
    float projectionMatrix[16];
    float modelViewMatrix[16];
    float depthRange[2];
    float padding[2];  // 16-byte alignment
};

struct vkViewPSUniform_t {
    float clipPlane[4];
    float alphaClip[2];
    float padding[2];  // 16-byte alignment
};

struct vkStageUniform_t {
    float color[4];
};

struct vkSkyboxVSUniform_t {
    float eyePos[4];
};

struct vkSkyboxPSUniform_t {
    float colorTint[4];
};

//----------------------------------------------------------------------------
// Pipeline cache key
//----------------------------------------------------------------------------
struct vkPipelineKey_t {
    uint32_t shaderType;      // vkShaderType_t
    uint32_t blendSrc;        // GLS_SRCBLEND_BITS
    uint32_t blendDst;        // GLS_DSTBLEND_BITS
    uint32_t depthFlags;      // TEST, WRITE, EQUAL
    uint32_t cullMode;        // CT_*
    uint32_t polygonMode;     // FILL, LINE
    uint32_t sampleCount;     // MSAA samples

    bool operator==(const vkPipelineKey_t& other) const {
        return shaderType == other.shaderType &&
               blendSrc == other.blendSrc &&
               blendDst == other.blendDst &&
               depthFlags == other.depthFlags &&
               cullMode == other.cullMode &&
               polygonMode == other.polygonMode &&
               sampleCount == other.sampleCount;
    }
};

// Hash function for pipeline key
namespace std {
    template<>
    struct hash<vkPipelineKey_t> {
        size_t operator()(const vkPipelineKey_t& k) const {
            return ((size_t)k.shaderType) |
                   ((size_t)k.blendSrc << 8) |
                   ((size_t)k.blendDst << 16) |
                   ((size_t)k.depthFlags << 24) |
                   ((size_t)k.cullMode << 32) |
                   ((size_t)k.polygonMode << 40) |
                   ((size_t)k.sampleCount << 48);
        }
    };
}

//----------------------------------------------------------------------------
// Pipeline state management
//----------------------------------------------------------------------------
struct vkPipelineState_t {
    // Pipeline layout (shared across all pipelines)
    VkPipelineLayout pipelineLayout;

    // Descriptor set layouts
    VkDescriptorSetLayout setLayouts[3];  // Set 0: View, Set 1: Material, Set 2: Textures

    // Descriptor pool
    VkDescriptorPool descriptorPool;

    // Pipeline cache (hash map)
    std::unordered_map<vkPipelineKey_t, VkPipeline> pipelineCache;

    // Shader modules
    VkShaderModule shaderModules[VK_SHADER_COUNT * 2];  // VS + PS per shader type
};

//----------------------------------------------------------------------------
// Runtime state tracking
//----------------------------------------------------------------------------
struct vkRunState_t {
    // Transform matrices
    float projectionMatrix[16];
    float modelViewMatrix[16];
    float depthRange[2];

    // Clip plane and alpha test
    float clipPlane[4];
    float alphaClip[2];

    // Current state
    uint32_t stateMask;        // GLS_* flags
    int cullMode;              // CT_* flags

    // Current bindings
    VkPipeline currentPipeline;
    VkDescriptorSet boundSets[3];

    // Dirty flags
    qboolean viewVSDirty;
    qboolean viewPSDirty;

    // Viewport and scissor
    int viewportX, viewportY, viewportWidth, viewportHeight;
};

//----------------------------------------------------------------------------
// Global state
//----------------------------------------------------------------------------
extern vkPipelineState_t g_vkPipelines;
extern vkRunState_t g_vkRunState;

//----------------------------------------------------------------------------
// State management functions
//----------------------------------------------------------------------------

// Initialize pipeline state
void VK_InitPipelineState();

// Destroy pipeline state
void VK_DestroyPipelineState();

// Create descriptor set layouts
void VK_CreateDescriptorSetLayouts();

// Create pipeline layout
void VK_CreatePipelineLayout();

// Create descriptor pool
void VK_CreateDescriptorPool();

// Load shader modules
void VK_LoadShaderModules();

// Get or create pipeline
VkPipeline VK_GetOrCreatePipeline(const vkPipelineKey_t& key);

// Create pipeline
VkPipeline VK_CreatePipeline(const vkPipelineKey_t& key);

// Pre-create common pipelines
void VK_PreCreatePipelines();

// Get vertex input state for shader type
void VK_GetVertexInputState(vkShaderType_t shaderType,
                            VkPipelineVertexInputStateCreateInfo* vertexInputInfo,
                            std::vector<VkVertexInputBindingDescription>& bindings,
                            std::vector<VkVertexInputAttributeDescription>& attributes);

// Map GLS_* flags to Vulkan state
void VK_MapBlendState(unsigned long stateMask, VkPipelineColorBlendAttachmentState* blendState);
void VK_MapDepthState(unsigned long stateMask, VkPipelineDepthStencilStateCreateInfo* depthState);
void VK_MapRasterState(int cullMode, unsigned long stateMask, VkPipelineRasterizationStateCreateInfo* rasterState);

// Build pipeline key from current state
vkPipelineKey_t VK_BuildPipelineKey(vkShaderType_t shaderType);

