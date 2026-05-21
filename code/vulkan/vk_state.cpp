#include "vk_common.h"
#include "vk_state.h"
#include "vk_driver.h"
#include "vk_image.h"
#include "vk_shaders.h"
#include "vk_drawdata.h"

//----------------------------------------------------------------------------
// Translate GLS_* state bitmasks to Vulkan pipeline state
//----------------------------------------------------------------------------

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
    const VkPipelineColorBlendStateCreateInfo* colorBlendState )
{
    // Dynamic state: viewport, scissor, depth range, depth bounds
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStageCount;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = vertexInputState;
    pipelineInfo.pInputAssemblyState = inputAssemblyState;
    pipelineInfo.pViewportState = viewportState;
    pipelineInfo.pRasterizationState = rasterizationState;
    pipelineInfo.pMultisampleState = multisampleState;
    pipelineInfo.pDepthStencilState = depthStencilState;
    pipelineInfo.pColorBlendState = colorBlendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    VK_CHECK( vkCreateGraphicsPipelines( g_vkDevice, g_vkPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ) );
    return pipeline;
}

void VKDRV_CreateDepthPipelines( VkPipeline* pipelines )
{
    // Vulkan integrates depth state into the full PSO.
    // Pipeline selection happens through the generic stage pipelines
    // created in InitGenericStageRenderData, which already include
    // depth/stencil state as part of VkGraphicsPipelineCreateInfo.
    // This function is kept for API compatibility but is a no-op.
    (void)pipelines;
}

void VKDRV_CreateRasterPipelines( VkPipeline* pipelines )
{
    // Vulkan integrates rasterizer state into the full PSO.
    // Pipeline selection happens through the generic stage pipelines
    // created in InitGenericStageRenderData.
    // This function is kept for API compatibility but is a no-op.
    (void)pipelines;
}

VkPipeline VKDRV_SelectPipeline( unsigned long depthState, int cullMode, qboolean wireframe, int blendState )
{
    unsigned long rasterState = 0;
    if ( wireframe )
        rasterState |= VK_RASTERIZER_FLAG_BACK; // VK_RASTERIZER_FLAG_BACK bit = wireframe
    if ( cullMode == CT_FRONT_SIDED )
        rasterState |= VK_RASTERIZER_FLAG_FRONT;
    else if ( cullMode == CT_TWO_SIDED )
        rasterState |= VK_RASTERIZER_FLAG_FRONT | VK_RASTERIZER_FLAG_BACK;
    if ( g_vkRunState.polyOffset )
        rasterState |= VK_RASTERIZER_FLAG_POLY_OFFSET;

    // Include alpha test in the pipeline key so alpha-tested surfaces get
    // distinct pipeline variants from blended surfaces with the same blend factors.
    // Index encoding: depthState[rasterState[blendState[alphaTest]]]
    // Shift amounts derived from enum sizes to avoid magic numbers.
    static constexpr int kAlphaTestShift = 0;
    static constexpr int kBlendStateShift = kAlphaTestShift + 2; // 2 bits for alphaTest (4 values)
    static constexpr int kRasterStateShift = kBlendStateShift + 2; // 2 bits for blendState (4 values)
    static constexpr int kDepthStateShift = kRasterStateShift + 3; // 3 bits for rasterState (8 values)
    int index = (int)( (depthState << kDepthStateShift)
                     | (rasterState << kRasterStateShift)
                     | (blendState << kBlendStateShift)
                     | g_vkRunState.alphaTest );

    int totalPipelines = VK_DEPTHSTATE_COUNT * VK_RASTERIZER_COUNT * VK_BLENDSTATE_COUNT * VK_ALPHATEST_COUNT;
    if ( index < 0 || index >= totalPipelines )
        return g_vkDrawState.genericStage.pipelineST;

    if ( g_vkDrawState.genericStage.pipelineCache[index] != VK_NULL_HANDLE )
        return g_vkDrawState.genericStage.pipelineCache[index];

    // Create pipeline variant on first use
    VkShaderModule vs = LoadShaderModule( "genericst_vs" );
    VkShaderModule ps = LoadShaderModule( "genericst_ps" );

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = CreateVertexShaderStage( vs );
    stages[1] = CreateFragmentShaderStage( ps );

    VkVertexInputBindingDescription viBindings[] = {
        { 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(vec2_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 2, 4, VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription viAttrs[] = {
        { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
        { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },
        { 2, 2, VK_FORMAT_R8G8B8A8_UNORM, 0 }
    };
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 3;
    vi.pVertexBindingDescriptions = viBindings;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = viAttrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    // Rasterizer state from bitmask
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = (rasterState & VK_RASTERIZER_FLAG_BACK) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    if ( (rasterState & VK_RASTERIZER_FLAG_FRONT) && (rasterState & VK_RASTERIZER_FLAG_BACK) )
    {
        rs.cullMode = VK_CULL_MODE_NONE; // two-sided: both flags set means no culling
    }
    else if ( rasterState & VK_RASTERIZER_FLAG_FRONT )
    {
        rs.cullMode = VK_CULL_MODE_FRONT_BIT;
    }
    else
    {
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
    }
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = (rasterState & VK_RASTERIZER_FLAG_POLY_OFFSET) != 0;
    rs.depthBiasConstantFactor = rs.depthBiasEnable ? 1.0f : 0;
    rs.depthBiasClamp = rs.depthBiasEnable ? 1.0f : 0;
    rs.depthBiasSlopeFactor = rs.depthBiasEnable ? 1.0f : 0;

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth state from bitmask
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = (depthState & VK_DEPTHSTATE_FLAG_TEST) != 0;
    ds.depthWriteEnable = (depthState & VK_DEPTHSTATE_FLAG_MASK) != 0;
    ds.depthCompareOp = (depthState & VK_DEPTHSTATE_FLAG_EQUAL) ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;

    // Blend state from GLS_*-derived blend mode
    VkPipelineColorBlendAttachmentState cb = {};
    cb.blendEnable = (blendState != VK_BLENDSTATE_OPAQUE) ? VK_TRUE : VK_FALSE;
    cb.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch ( blendState )
    {
    case VK_BLENDSTATE_ALPHA:
        cb.blendEnable = VK_TRUE;
        cb.srcColorBlendFactor = VKDRV_GetSrcBlendFactor( g_vkRunState.srcBlend );
        cb.dstColorBlendFactor = VKDRV_GetDstBlendFactor( g_vkRunState.dstBlend );
        cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_BLENDSTATE_ADDITIVE:
        cb.blendEnable = VK_TRUE;
        cb.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case VK_BLENDSTATE_MODULATE:
        cb.blendEnable = VK_TRUE;
        cb.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    default: // VK_BLENDSTATE_OPAQUE
        cb.blendEnable = VK_FALSE;
        break;
    }

    VkPipelineColorBlendStateCreateInfo cbState = {};
    cbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cb;

    g_vkDrawState.genericStage.pipelineCache[index] = VKDRV_CreatePipeline(
        g_vkPipelineLayout, g_vkRenderPass,
        stages, 2, &vi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        &ia, &vp, &rs, &ms, &ds, &cbState );

    // Do not destroy shader modules here — they are cached by LoadShaderModule
    // and will be destroyed by DestroyShaders() during shutdown.

    return g_vkDrawState.genericStage.pipelineCache[index];
}

//----------------------------------------------------------------------------
// D3D11-style blend factor mapping
//----------------------------------------------------------------------------

VkBlendFactor VKDRV_GetSrcBlendFactor( int qConstant )
{
    switch ( qConstant )
    {
    case GLS_SRCBLEND_ZERO:               return VK_BLEND_FACTOR_ZERO;
    case GLS_SRCBLEND_ONE:                return VK_BLEND_FACTOR_ONE;
    case GLS_SRCBLEND_DST_COLOR:          return VK_BLEND_FACTOR_DST_COLOR;
    case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GLS_SRCBLEND_SRC_ALPHA:          return VK_BLEND_FACTOR_SRC_ALPHA;
    case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GLS_SRCBLEND_DST_ALPHA:          return VK_BLEND_FACTOR_DST_ALPHA;
    case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GLS_SRCBLEND_ALPHA_SATURATE:     return VK_BLEND_FACTOR_SRC_ALPHA; // GL_ALPHA_SATURATE = min(srcA, 1-dstA) has no Vulkan equivalent
    default:                              return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendFactor VKDRV_GetDstBlendFactor( int qConstant )
{
    switch ( qConstant )
    {
    case GLS_DSTBLEND_ZERO:               return VK_BLEND_FACTOR_ZERO;
    case GLS_DSTBLEND_ONE:                return VK_BLEND_FACTOR_ONE;
    case GLS_DSTBLEND_SRC_COLOR:          return VK_BLEND_FACTOR_SRC_COLOR;
    case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GLS_DSTBLEND_SRC_ALPHA:          return VK_BLEND_FACTOR_SRC_ALPHA;
    case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GLS_DSTBLEND_DST_ALPHA:          return VK_BLEND_FACTOR_DST_ALPHA;
    case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:                              return VK_BLEND_FACTOR_ONE;
    }
}

//----------------------------------------------------------------------------
// CommitRasterizerState -- mirrors D3D11's CommitRasterizerState
//----------------------------------------------------------------------------

void VKDRV_CommitRasterizerState( int cullMode, qboolean polyOffset, qboolean outline )
{
    g_vkRunState.cullMode = cullMode;
    g_vkRunState.wireframe = outline;
    g_vkRunState.polyOffset = polyOffset;
}

//----------------------------------------------------------------------------
// UpdateViewState -- uploads VS uniform buffer if dirty
//----------------------------------------------------------------------------

void UpdateViewState()
{
    if ( !g_vkRunState.vsDirtyConstants )
        return;

    vkViewVSUniformBuffer_t* data = g_vkDrawState.viewRenderData.vsUniformData[g_vkCurrentFrame];
    if ( data )
    {
        Com_Memcpy( data, &g_vkRunState.vsConstants, sizeof( vkViewVSUniformBuffer_t ) );
    }
    g_vkRunState.vsDirtyConstants = qfalse;
}

//----------------------------------------------------------------------------
// UpdateMaterialState -- uploads PS uniform buffer if dirty
//----------------------------------------------------------------------------

void UpdateMaterialState()
{
    if ( !g_vkRunState.psDirtyConstants )
        return;

    vkViewPSUniformBuffer_t* data = g_vkDrawState.viewRenderData.psUniformData[g_vkCurrentFrame];
    if ( data )
    {
        Com_Memcpy( data, &g_vkRunState.psConstants, sizeof( vkViewPSUniformBuffer_t ) );
    }
    g_vkRunState.psDirtyConstants = qfalse;
}

//----------------------------------------------------------------------------
// VKDrv_SetState -- main state transition function
//----------------------------------------------------------------------------

void VKDrv_SetState( unsigned long stateBits )
{
    unsigned long diff = stateBits ^ g_vkRunState.stateMask;
    if ( !diff )
        return;

    // Depth state
    unsigned long newDepthStateMask = 0;
    if ( stateBits & GLS_DEPTHFUNC_EQUAL )
        newDepthStateMask |= VK_DEPTHSTATE_FLAG_EQUAL;
    if ( stateBits & GLS_DEPTHMASK_TRUE )
        newDepthStateMask |= VK_DEPTHSTATE_FLAG_MASK;
    if ( !(stateBits & GLS_DEPTHTEST_DISABLE) )
        newDepthStateMask |= VK_DEPTHSTATE_FLAG_TEST;

    if ( newDepthStateMask != g_vkRunState.depthStateMask )
    {
        // In Vulkan, pipeline selection handles depth state
        // This would select the appropriate pre-created pipeline
        g_vkRunState.depthStateMask = newDepthStateMask;
    }

    // Alpha test state -- track separately from blend state to avoid conflicts.
    // GLS_ATEST_* bits (0x70000000) are in a different range than blend bits,
    // but when alpha test is active, blend factors may not match known patterns,
    // causing the blend state to resolve to OPAQUE. By tracking alpha test
    // independently, we ensure the pipeline cache correctly distinguishes
    // alpha-tested surfaces from blended surfaces.
    {
        int newAlphaTest = VK_ALPHATEST_NONE;
        switch ( stateBits & GLS_ATEST_BITS )
        {
        case GLS_ATEST_GT_0:
            newAlphaTest = VK_ALPHATEST_GT_0;
            break;
        case GLS_ATEST_LT_80:
            newAlphaTest = VK_ALPHATEST_LT_80;
            break;
        case GLS_ATEST_GE_80:
            newAlphaTest = VK_ALPHATEST_GE_80;
            break;
        default:
            break;
        }
        g_vkRunState.alphaTest = newAlphaTest;
    }

    // Blend state -- derive from GLS_SRCBLEND_* and GLS_DSTBLEND_* bits
    {
        int srcBlend = (stateBits & GLS_SRCBLEND_BITS);
        int dstBlend = (stateBits & GLS_DSTBLEND_BITS);
        int newBlendState = VK_BLENDSTATE_OPAQUE;

        if ( srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE )
        {
            newBlendState = VK_BLENDSTATE_ADDITIVE;
        }
        else if ( srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE )
        {
            newBlendState = VK_BLENDSTATE_MODULATE;
        }
        else if ( srcBlend != GLS_SRCBLEND_ZERO && dstBlend != GLS_DSTBLEND_ZERO )
        {
            newBlendState = VK_BLENDSTATE_ALPHA;
        }

        g_vkRunState.blendState = newBlendState;
        g_vkRunState.srcBlend = srcBlend;
        g_vkRunState.dstBlend = dstBlend;
    }

    // Wireframe (poly mode line)
    if ( diff & GLS_POLYMODE_LINE )
    {
        g_vkRunState.wireframe = (stateBits & GLS_POLYMODE_LINE) ? qtrue : qfalse;
    }

    // Alpha test → update shader uniform when the tracked state changes.
    // Uses g_vkRunState.alphaTest (set above) instead of re-parsing bits.
    if ( g_vkRunState.alphaTest != (g_vkRunState.prevAlphaTest) )
    {
        const float alphaEps = 0.00001f;
        switch ( g_vkRunState.alphaTest )
        {
        case VK_ALPHATEST_NONE:
            g_vkRunState.psConstants.alphaClip[0] = 1;
            g_vkRunState.psConstants.alphaClip[1] = 0;
            break;
        case VK_ALPHATEST_GT_0:
            g_vkRunState.psConstants.alphaClip[0] = 1;
            g_vkRunState.psConstants.alphaClip[1] = alphaEps;
            break;
        case VK_ALPHATEST_LT_80:
            g_vkRunState.psConstants.alphaClip[0] = -1;
            g_vkRunState.psConstants.alphaClip[1] = 0.5f;
            break;
        case VK_ALPHATEST_GE_80:
            g_vkRunState.psConstants.alphaClip[0] = 1;
            g_vkRunState.psConstants.alphaClip[1] = 0.5f;
            break;
        default:
            ASSERT(0);
            break;
        }
        g_vkRunState.psDirtyConstants = qtrue;
        g_vkRunState.prevAlphaTest = g_vkRunState.alphaTest;
    }

    g_vkRunState.stateMask = stateBits;
}

void VKDrv_ResetState2D( void )
{
    // Reset to default 2D state: no depth test, no culling, no blending
    VKDrv_SetState( 0 );
    g_vkRunState.cullMode = -1;
    g_vkRunState.wireframe = qfalse;
}

void VKDrv_ResetState3D( void )
{
    // Reset to default 3D state
    VKDrv_SetState( GLS_DEFAULT );
    g_vkRunState.cullMode = -1;
    g_vkRunState.wireframe = qfalse;
}

//----------------------------------------------------------------------------
// Init/Destroy draw state
//----------------------------------------------------------------------------

void VKDRV_InitDrawState()
{
    Com_Memset( &g_vkRunState, 0, sizeof( g_vkRunState ) );
    Com_Memset( &g_vkDrawState, 0, sizeof( g_vkDrawState ) );

    Com_Memcpy( g_vkRunState.vsConstants.modelViewMatrix, s_identityMatrix, sizeof(float) * 16 );
    Com_Memcpy( g_vkRunState.vsConstants.projectionMatrix, s_identityMatrix, sizeof(float) * 16 );
    g_vkRunState.vsConstants.depthRange[0] = 0;
    g_vkRunState.vsConstants.depthRange[1] = 1;
    g_vkRunState.stateMask = 0;
    g_vkRunState.wireframe = qfalse;
    g_vkRunState.polyOffset = qfalse;
    g_vkRunState.blendState = VK_BLENDSTATE_OPAQUE;
    g_vkRunState.alphaTest = VK_ALPHATEST_NONE;
    g_vkRunState.prevAlphaTest = -1; // Forces initial uniform update
    g_vkRunState.vsDirtyConstants = qtrue;
    g_vkRunState.psDirtyConstants = qtrue;
    g_vkRunState.cullMode = -1;

    // Create all GPU resources
    VKDRV_InitImages();
    VKDRV_InitShaders();
    InitQuadRenderData( &g_vkDrawState.quadRenderData );
    InitSkyBoxRenderData( &g_vkDrawState.skyBoxRenderData );
    InitViewRenderData( &g_vkDrawState.viewRenderData );
    InitGenericStageRenderData( &g_vkDrawState.genericStage );
    InitFogRenderData( &g_vkDrawState.fogRenderData );
    InitLightmapRenderData( &g_vkDrawState.lightmapRenderData );
    InitTessBuffers( &g_vkDrawState.tessBufs );
}

void VKDRV_DestroyDrawState()
{
    DestroyTessBuffers( &g_vkDrawState.tessBufs );
    DestroyGenericStageRenderData( &g_vkDrawState.genericStage );
    DestroyFogRenderData( &g_vkDrawState.fogRenderData );
    DestroyLightmapRenderData( &g_vkDrawState.lightmapRenderData );
    DestroyViewRenderData( &g_vkDrawState.viewRenderData );
    DestroySkyBoxRenderData( &g_vkDrawState.skyBoxRenderData );
    DestroyQuadRenderData( &g_vkDrawState.quadRenderData );
    VKDRV_DestroyShaders();
    VKDRV_DestroyImages();

    Com_Memset( &g_vkRunState, 0, sizeof( g_vkRunState ) );
    Com_Memset( &g_vkDrawState, 0, sizeof( g_vkDrawState ) );
}
