#ifndef __VK_STATE_H__
#define __VK_STATE_H__

#include "vk_common.h"

//----------------------------------------------------------------------------
// State bitmask constants (mirror GLS_* → Vulkan pipeline state)
//----------------------------------------------------------------------------

enum {
    VK_DEPTHSTATE_FLAG_TEST   = 1,
    VK_DEPTHSTATE_FLAG_MASK   = 2,
    VK_DEPTHSTATE_FLAG_EQUAL  = 4,
    VK_DEPTHSTATE_COUNT       = 8
};

enum {
    VK_RASTERIZER_FLAG_FRONT  = 1,
    VK_RASTERIZER_FLAG_BACK   = 2,
    VK_RASTERIZER_FLAG_POLY_OFFSET = 4,
    VK_RASTERIZER_COUNT       = 8
};

// Blend state variants (for pipeline cache key)
enum {
    VK_BLENDSTATE_OPAQUE    = 0,
    VK_BLENDSTATE_ALPHA     = 1,   // src_alpha * color + (1 - src_alpha) * dst
    VK_BLENDSTATE_ADDITIVE  = 2,   // 1 * color + 1 * dst
    VK_BLENDSTATE_MODULATE  = 3,   // dst_color * color + 1 * dst (dlight non-additive)
    VK_BLENDSTATE_COUNT     = 4
};

// Alpha test variants (for pipeline cache key)
enum {
    VK_ALPHATEST_NONE = 0,
    VK_ALPHATEST_GT_0 = 1,
    VK_ALPHATEST_LT_80 = 2,
    VK_ALPHATEST_GE_80 = 3,
    VK_ALPHATEST_COUNT = 4
};

// Pipeline layout -- shared across all pipelines
extern VkPipelineLayout g_vkPipelineLayout;

//----------------------------------------------------------------------------
// Dynamic uniform buffer layouts (matches push constant + UBO pattern)
//----------------------------------------------------------------------------

// Must be 256 bytes for uniform buffer alignment
struct vkViewVSUniformBuffer_t {
    float projectionMatrix[16];
    float modelViewMatrix[16];
    float depthRange[2];
    float padding[2];
};

struct vkViewPSUniformBuffer_t {
    float clipPlane[4];
    float alphaClip[2];
    float padding[2];
};

struct vkSkyBoxVSUniformBuffer_t {
    float eyePos[4];
};

struct vkSkyBoxPSUniformBuffer_t {
    float color[4];
};

struct vkQuadUniformBuffer_t {
    float color[4];
};

//----------------------------------------------------------------------------
// Vulkan pipeline state structures
//----------------------------------------------------------------------------

// @pjb: Vulkan-equivalent of d3dImage_t
struct vkImage_t {
    VkImage         image;
    VmaAllocation   allocation;
    VkImageView     imageView;
    VkSampler       sampler;
    VkImageLayout   layout;
    VkFormat        format;
    int             width;
    int             height;
    int             frameUsed;
    qboolean        dynamic;
};

// Render data for 2D quad drawing
struct vkQuadRenderData_t {
    VkPipeline      vertexShader;   // really VkPipeline (graphics pipeline)
    VkPipeline      pipeline;
    VkPipeline      debugPipeline;  // single-position wireframe for debug draws
    VkPipeline      shadowPipeline; // stencil shadow volume with depth bias
    VkPipelineLayout pipelineLayout;

    // Vertex data (static 2-corner quad)
    VkBuffer        vertexBuffer;
    VmaAllocation   vertexAllocation;
    VkBuffer        indexBuffer;
    VmaAllocation   indexAllocation;

    // Uniform buffer
    VkBuffer        uniformBuffer;
    VmaAllocation   uniformAllocation;
    vkQuadUniformBuffer_t* uniformData;
};

// Render data for skybox
struct vkSkyBoxRenderData_t {
    VkPipeline      pipeline;

    // Vertex buffer (static unit cube)
    VkBuffer        vertexBuffer;
    VmaAllocation   vertexAllocation;

    // Uniform buffers
    VkBuffer        vsUniformBuffer;
    VmaAllocation   vsUniformAllocation;
    vkSkyBoxVSUniformBuffer_t* vsUniformData;

    VkBuffer        psUniformBuffer;
    VmaAllocation   psUniformAllocation;
    vkSkyBoxPSUniformBuffer_t* psUniformData;
};

// View-level uniform buffers (ring buffer for double-buffering)
struct vkViewRenderData_t {
    VkBuffer        vsUniformBuffer[VK_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation   vsUniformAllocation[VK_MAX_FRAMES_IN_FLIGHT];
    vkViewVSUniformBuffer_t* vsUniformData[VK_MAX_FRAMES_IN_FLIGHT];

    VkBuffer        psUniformBuffer[VK_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation   psUniformAllocation[VK_MAX_FRAMES_IN_FLIGHT];
    vkViewPSUniformBuffer_t* psUniformData[VK_MAX_FRAMES_IN_FLIGHT];
};

// Generic stage rendering pipelines
struct vkGenericStageRenderData_t {
    // Single-texture pipeline
    VkPipeline      pipelineST;

    // Multi-texture pipeline
    VkPipeline      pipelineMT;

    // Lazy pipeline cache: creates variants on first use
    // Indexed by (depthState << (2 + 2 + 2)) | (rasterState << (2 + 2)) | (blendState << 2) | alphaTest
    VkPipeline      pipelineCache[VK_DEPTHSTATE_COUNT * VK_RASTERIZER_COUNT * VK_BLENDSTATE_COUNT * VK_ALPHATEST_COUNT];
};

// @pjb: circular buffer for tessellation data
struct vkCircularBuffer_t {
    VkBuffer        buffer;
    VmaAllocation   allocation;
    void*           mappedData;     // persistently mapped
    unsigned        currentOffset;
    unsigned        nextOffset;
    unsigned        size;
};

// GPU caches for stageVars_t
struct vkTessStageBuffers_t {
    vkCircularBuffer_t texCoords[NUM_TEXTURE_BUNDLES];
    vkCircularBuffer_t colors;
};

struct vkTessFogBuffers_t {
    vkCircularBuffer_t texCoords;
    vkCircularBuffer_t colors;
};

struct vkTessLightProjBuffers_t {
    vkCircularBuffer_t indexes;
    vkCircularBuffer_t texCoords;
    vkCircularBuffer_t colors;
};

// GPU caches for shaderCommands_t
struct vkTessBuffers_t {
    vkCircularBuffer_t indexes;
    vkCircularBuffer_t xyz;
    vkTessStageBuffers_t stages[MAX_SHADER_STAGES];
    vkTessLightProjBuffers_t dlights[MAX_DLIGHTS];
    vkTessFogBuffers_t fog;
};

//----------------------------------------------------------------------------
// Draw state -- all persistent GPU objects
//----------------------------------------------------------------------------

struct vkDrawState_t {
    vkQuadRenderData_t      quadRenderData;
    vkSkyBoxRenderData_t    skyBoxRenderData;
    vkViewRenderData_t      viewRenderData;

    vkTessBuffers_t         tessBufs;
    vkGenericStageRenderData_t genericStage;

    // Cached pipeline states (selected by bitmask)
    VkPipeline              depthPipelines[VK_DEPTHSTATE_COUNT];
    VkPipeline              rasterPipelines[VK_RASTERIZER_COUNT];
};

//----------------------------------------------------------------------------
// Run state -- per-frame mutable state with dirty tracking
//----------------------------------------------------------------------------

struct vkRunState_t {
    vkViewVSUniformBuffer_t vsConstants;
    vkViewPSUniformBuffer_t psConstants;
    unsigned long           stateMask;    // combination of GLS_* flags
    int                     cullMode;     // CT_* enum only (CT_BACK_SIDED / CT_FRONT_SIDED / CT_TWO_SIDED)
    qboolean                wireframe;    // from GLS_POLYMODE_LINE or debug outline
    qboolean                polyOffset;   // from shader polygonOffset
    unsigned long           depthStateMask;
    int                     blendState;   // VK_BLENDSTATE_* enum (for cache key)
    int                     alphaTest;    // VK_ALPHATEST_* enum (tracked separately from blend)
    int                     prevAlphaTest; // previous frame's alphaTest for dirty detection
    int                     srcBlend;     // GLS_SRCBLEND_* constant (for factor mapping)
    int                     dstBlend;     // GLS_DSTBLEND_* constant (for factor mapping)
    qboolean                vsDirtyConstants;
    qboolean                psDirtyConstants;
    int                     viewportLeft;
    int                     viewportTop;
    int                     viewportWidth;
    int                     viewportHeight;
};

//----------------------------------------------------------------------------
// Global state
//----------------------------------------------------------------------------

extern vkRunState_t   g_vkRunState;
extern vkDrawState_t  g_vkDrawState;

//----------------------------------------------------------------------------
// Internal APIs
//----------------------------------------------------------------------------

void InitDrawState();
void DestroyDrawState();

void CommitRasterizerState( int cullMode, qboolean polyOffset, qboolean outline );
void UpdateViewState();
void UpdateMaterialState();

VkBlendFactor VKDRV_GetSrcBlendFactor( int qConstant );
VkBlendFactor VKDRV_GetDstBlendFactor( int qConstant );

VkPipeline VKDRV_CreatePipeline(
    VkPipelineLayout layout,
    VkRenderPass renderPass,
    const VkPipelineShaderStageCreateInfo* shaderStages,
    uint32_t shaderStageCount,
    const VkPipelineVertexInputStateCreateInfo* vertexInputState,
    VkPrimitiveTopology topology,
    const VkPipelineInputAssemblyStateCreateInfo* inputAssemblyState,
    const VkPipelineViewportStateCreateInfo* viewportState,
    const VkPipelineRasterizationStateCreateInfo* rasterizationState,
    const VkPipelineMultisampleStateCreateInfo* multisampleState,
    const VkPipelineDepthStencilStateCreateInfo* depthStencilState,
    const VkPipelineColorBlendStateCreateInfo* colorBlendState );

VkPipeline VKDRV_SelectPipeline( unsigned long depthState, int cullMode, qboolean wireframe, int blendState );

#endif
